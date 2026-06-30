/*
 * control_stage.h
 *
 *  Created on: Jun 30, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_CONTROL_STAGE_H_
#define INCLUDE_MMPILOT_CONTROL_STAGE_H_

#include <mmpilot/stage.h>
#include <mmpilot/control.h>
#include <mmpilot/gyro.h>
#include <mmpilot/transform.h>


namespace mmpilot {

template<typename T>
class PDControl {
public:
	Vec2f PD = Vec2f(1, -1);		// (P, D)

	float gain = 1;					// global gain

	PDControl() = default;
	PDControl(float gain_) : gain(gain_) {}

	T update(const T& error, const T& derivative)
	{
		return (error * PD.x() + derivative * PD.y()) * gain;
	}
};


class ControlStage : public Stage {
public:
	int max_yaw = 50;			// RC offset
	int max_angle = 200;		// RC offset
	int max_throttle = 700;		// RC offset

	int override_channel = (4 + 5) - 1;		// AUX


	ControlStage(MSP2* msp_) : Stage("control"), msp(msp_) {}

	Gyro::State gyro;

	PDControl<float> yaw_control = PDControl<float>(2);
	PDControl<Vec2f> angle_control = PDControl<Vec2f>(1);
	PDControl<float> throttle_control = PDControl<float>(0.1);

	float z_speed = 1;					// scale / sec
	float yaw_rate = 0;					// deg / sec
	Vec2f xy_speed = Vec2f(0, 0);		// pix / sec

	float out_yawrate = 0;				// ticks
	float out_throttle = 0;				// 0 to 1
	Vec2f out_angle = Vec2f(0, 0);		// ticks

	float base_throttle = 0.5;			// 0 to 1
	float base_throttle_gain = 0.2;

	Transform2D odom;

protected:
	void exec() override
	{
		gyro = get_input<Gyro::State>("gyro");

		if(auto in = find_input<ConstPointer>("control"))
		{
			if(auto cmd = in->get<VelocityControl>()) {
				exec_vel(*cmd);
			}
			else if(auto cmd = in->get<PositionControl>()) {
				exec_pos(*cmd);
			}
		}
		else {
			// fallback to hover on missing control input
			std::cout << "WARN: Missing control input, fallback to hover" << std::endl;
			exec_pos(PositionControl());
		}
	}

	void exec_vel(const VelocityControl& cmd)
	{
		if(mode != VEL) {
			mode = VEL;
			std::cout << "INFO: Switching to VELOCITY control mode" << std::endl;
		}
		// TODO
	}

	void exec_pos(const PositionControl& cmd)
	{
		if(mode != POS) {
			mode = POS;
			std::cout << "INFO: Switching to POSITION control mode" << std::endl;
		}

	}

	void send()
	{
		out_throttle = std::min(std::max(out_throttle, 0.f), 1.f);

		std::array<uint16_t, 8> rc = {};
		rc[0] = 1500 + std::min(std::max(int(out_angle.x()), -max_angle), max_angle),	// roll
		rc[1] = 1500 + std::min(std::max(int(out_angle.y()), -max_angle), max_angle),	// pitch
		rc[2] = 1000 + std::min(std::max(int(out_throttle * 1000), 0), max_throttle),	// throttle
		rc[3] = 1500 + std::min(std::max(int(out_yawrate), -max_yaw), max_yaw),			// yaw

		std::cout << "RC_OVERRIDE: " << to_string(rc) << std::endl;

		if(msp) {
			msp->send_raw_rc(rc);
		}
	}

	void enable()
	{
		active = true;

		// reset odometry
		odom = Transform2D();

		// keep current yaw
		target_yaw = angle_norm_180(gyro.get_rpy().z());

		std::cout << "Control: Enabled with yaw " << target_yaw << " deg" << std::endl;
	}

	void disable()
	{
		active = false;

		std::cout << "Control: Disabled" << std::endl;
	}

private:
	enum mode_e {
		NONE, VEL, POS
	} mode = NONE;

	bool active = false;

	int64_t last_ts = 0;				// us

	float target_yaw = 0;				// deg

	MSP2* msp = nullptr;

};





} // mmpilot

#endif /* INCLUDE_MMPILOT_CONTROL_STAGE_H_ */
