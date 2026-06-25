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


class TestControl : public Pipeline {
public:
	int max_yaw = 100;			// RC offset
	int max_angle = 200;		// RC offset
	int max_throttle = 600;		// RC offset

	int override_channel = 4 + 5 - 1;		// AUX

	float yaw_gain = 10;				// deg / sec to RC range
	float angle_gain = 5;				// pix to RC range
	float start_throttle = 0.3;			// 0 to 1

	Vec2f yaw_param = Vec2f(1, -0.5);				// 1 / sec
	Vec2f angle_param = Vec2f(1, -0.5);				// 1 / pix
	Vec2f throttle_param = Vec2f(1, -0.5);			// 1 / sec

	float z_speed = 1;					// scale / sec
	float yaw_rate = 0;					// rad / sec
	Vec2f xy_speed = Vec2f(0, 0);		// pix / sec

	float out_yawrate = 0;				// deg / sec
	float out_throttle = 0;				// 0 to 1
	Vec2f out_angle = Vec2f(0, 0);		// pix

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

		std::cout << "INFO: Enabled with yaw " << target_yaw << " deg" << std::endl;
	}

	void disable() {
		active = false;

		std::cout << "INFO: Disabled" << std::endl;
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
		std::cout << "Delta: " << to_string(delta) << " (overlap = " << delta.overlap << ")" << std::endl;

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

			out_angle = delta.translation() * angle_param.x() + xy_speed * angle_param.y();

			out_angle = get_rotation_matrix(deg2rad(RPY.z())) * out_angle;

			out_throttle += (1 - delta.scale()) * throttle_param.x() + (z_speed - 1) * throttle_param.y();

			out_throttle  = std::min(std::max(out_throttle, 0.f), 1.f);

			out_yawrate = (target_yaw - yaw_deg) * yaw_param.x() + rad2deg(yaw_rate) * yaw_param.y();

			std::cout << "Control: roll = " << out_angle.x() << ", pitch = " << out_angle.y() << ", yaw = " << out_yawrate << " deg/s, throttle = " << out_throttle << std::endl;
		}
		else {
			out_yawrate = 0;
			out_throttle = start_throttle;
			out_angle = Vec2f(0, 0);
		}

		std::array<uint16_t, 8> rc = {};
		rc[0] = 1500 + std::min(std::max(int(out_angle.x() * angle_gain), -max_angle), max_angle),	// roll
		rc[1] = 1500 + std::min(std::max(int(out_angle.y() * angle_gain), -max_angle), max_angle),	// pitch
		rc[2] = 1000 + std::min(std::max(int(out_throttle * 1000), 0), max_throttle),				// throttle
		rc[3] = 1500 + std::min(std::max(int(out_yawrate * yaw_gain), -max_yaw), max_yaw),			// yaw

		std::cout << "RC_OVERRIDE: " << to_string(rc) << std::endl;

		if(msp) {
			msp->send_raw_rc(rc);
		}

		if(delta.overlap < 0.25 || delta.translation().norm() > 50)
		{
			std::cout << "INFO: rebase with overlap " << delta.overlap << std::endl;
			rebase();
		}

//		show(display, source);
//		show(display, stage[0]->solver.tex_debug);
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
			std::cout << "RC: ts = " << rc->ts << ", roll = " << rc->ch[0] << ", pitch = " << rc->ch[1] << ", yaw = " << rc->ch[2] << ", throttle = " << rc->ch[3] << std::endl;
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
