/*
 * test_control.h
 *
 *  Created on: Jun 24, 2026
 *      Author: mad
 */

#ifndef TEST_CONTROL_H_
#define TEST_CONTROL_H_

#include <mmpilot/beta_msp.h>

#include "pipeline2.h"


inline float exp_gain(const float state, const float value, const float gain)
{
	return state * (1 - gain) + value * gain;
}


template<typename T>
class PDControl {
public:
	Vec2f PD = Vec2f(1, -1);		// (P, D)

	float gain = 1;					// global scaling

	PDControl() = default;
	PDControl(float gain_) : gain(gain_) {}

	T update(const T& error, const T& derivative)
	{
		return (error * PD.x() + derivative * PD.y()) * gain;
	}
};


class TestControl : public Pipeline {
public:
	int max_yaw = 50;			// RC offset
	int max_angle = 150;		// RC offset
	int max_throttle = 600;		// RC offset

	int override_channel = 4 + 5 - 1;		// AUX

	PDControl<float> yaw_control = PDControl<float>(2);
	PDControl<Vec2f> angle_control = PDControl<Vec2f>(1);
	PDControl<float> throttle_control = PDControl<float>(0.1);

	float z_speed = 1;					// scale / sec
	float yaw_rate = 0;					// rad / sec
	Vec2f xy_speed = Vec2f(0, 0);		// pix / sec

	float out_yawrate = 0;				// deg / sec
	float out_throttle = 0;				// 0 to 1
	Vec2f out_angle = Vec2f(0, 0);		// pix

	float base_throttle = 0.5;			// 0 to 1
	float base_throttle_gain = 0.2;

	TestControl(MSP2Client* msp_)
		:	msp(msp_)
	{
	}

protected:
	void init(int width, int height) override
	{
		Pipeline::init(width, height);
	}

	void enable() {
		active = true;

		// keep current yaw
		target_yaw = gyro.get_rpy().z();

		rebase();

		std::cout << "Control: Enabled with yaw " << target_yaw << " deg" << std::endl;
	}

	void disable() {
		active = false;

		std::cout << "Control: Disabled" << std::endl;
	}

	void update() override
	{
		const float dt = last_ts ? (ts - last_ts) * 1e-6 : 0;		// [sec]
		last_ts = ts;

		const auto delta = get_params();
		if(!delta.valid()) {
			std::cout << "WARNING: delta not valid!" << std::endl;
			rebase();
			return;
		}
		std::cout << "Delta: " << to_string(delta) << " (overlap = " << delta.overlap << ", R = " << delta.R_norm << ", dt = " << dt << " sec)" << std::endl;

		yaw_rate = gyro.omega.z();

		if(dt > 0) {
			if(last_scale > 0) {
				z_speed = pow(delta.scale() / last_scale, 1 / dt);
			} else {
				z_speed = 1;
			}
			xy_speed = (delta.translation() - last_pos) / dt;
		}
		last_yaw = delta.yaw();
		last_scale = delta.scale();
		last_pos = delta.translation();

		std::cout << "Speed: xy = " << xy_speed.transpose() << " pix/s, yaw = " << rad2deg(yaw_rate) << " deg/s, z = " << z_speed << std::endl;

		if(active) {
			const Vec3f RPY = gyro.get_rpy();

			const float yaw_deg = RPY.z() - rad2deg(delta.yaw());	// TODO: sign correct?

			out_angle = angle_control.update(
					delta.translation(),
					-xy_speed
			);

			// transform xy control to roll / pitch
			out_angle = get_rotation_matrix(deg2rad(RPY.z() + 90)) * out_angle;

			out_throttle = base_throttle + throttle_control.update(
					1 - delta.scale(),
					z_speed - 1
			);

			// update base throttle
			base_throttle = exp_gain(base_throttle, out_throttle, base_throttle_gain * dt);

			out_yawrate = yaw_control.update(
					angle_norm_180(yaw_deg - target_yaw),
					rad2deg(-yaw_rate)
			);

			std::cout << "Control: roll = " << out_angle.x() << ", pitch = " << out_angle.y() << ", yaw = " << out_yawrate << ", throttle = " << out_throttle << " (base " << base_throttle << ")" << std::endl;
		}
		else {
			out_yawrate = 0;
			out_throttle = base_throttle;
			out_angle = Vec2f(0, 0);
		}

		out_throttle = std::min(std::max(out_throttle, 0.f), 1.f);

		std::array<uint16_t, 8> rc = {};
		rc[0] = 1500 + std::min(std::max(int(out_angle.x()), -max_angle), max_angle),	// roll
		rc[1] = 1500 + std::min(std::max(int(out_angle.y()), -max_angle), max_angle),	// pitch
		rc[2] = 1000 + std::min(std::max(int(out_throttle * 1000), 0), max_throttle),				// throttle
		rc[3] = 1500 + std::min(std::max(int(out_yawrate), -max_yaw), max_yaw),			// yaw

		std::cout << "RC_OVERRIDE: " << to_string(rc) << std::endl;

		if(msp) {
			msp->send_raw_rc(rc);
		}

		if(delta.overlap < 0.2 || delta.R_norm > 100)
		{
			std::cout << "Control: rebase with overlap " << delta.overlap << ", R_norm = " << delta.R_norm << std::endl;
			rebase();
		}

//		show(display, source);
//		show(display, stage[0]->solver.tex_debug);

//		if(!active) {
//			enable();
//		}
	}

	void rebase() override
	{
		Pipeline::rebase();

		last_yaw = 0;
		last_scale = 1;
		last_pos = Vec2f(0, 0);
	}

	void on_sample(std::shared_ptr<Sample> sample) override
	{
		Pipeline::on_sample(sample);

		if(auto rc = std::dynamic_pointer_cast<MSP2Client::RcPacket>(sample))
		{
			if(!active) {
				base_throttle = (float(rc->throttle()) - 1000) / 1000;
			}
			if(override_channel < rc->ch.size())
			{
				if(rc->ch.at(override_channel) > 1500) {
					if(!active) {
						enable();
					}
				} else {
					if(active) {
						disable();
					}
				}
			}
			std::cout << "RC: ts = " << rc->ts << ", roll = " << rc->roll() << ", pitch = " << rc->pitch() << ", yaw = " << rc->yaw() << ", throttle = " << rc->throttle() << std::endl;
		}
	}

private:
	bool active = false;
	int64_t last_ts = 0;				// us

	float target_yaw = 0;				// deg

	float last_yaw = 0;					// rad
	float last_scale = 1;
	Vec2f last_pos = Vec2f(0, 0);		// pix

	MSP2Client* msp = nullptr;

};



#endif /* TEST_CONTROL_H_ */
