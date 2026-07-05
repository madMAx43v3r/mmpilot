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
#include <mmpilot/affine.h>
#include <mmpilot/transform.h>


namespace mmpilot {

class ControlStage : public Stage {
public:
	int max_yaw = 50;			// RC offset
	int max_angle = 200;		// RC offset
	int max_throttle = 700;		// RC offset

	float yaw_gain = 1;
	float position_gain = 2;
	float throttle_gain = 0.1;

	float yawrate_gain = 1;
	float velocity_gain = 2;
	float vertical_gain = 0.1;

	float AGL_min = 1;					// sanity limit [m]

	int override_channel = 5;			// AUX
	int override_threshold = 1550;


	ControlStage(MSP2* msp_)
		:	Stage("control"), msp(msp_)
	{
	}

	Gyro::State gyro;
	Affine::Params affine;
	ImageVelocity velocity;

	float dt = 0;						// [sec]
	float AGL = 0;						// [m]
	float cam_yaw = 0;					// [rad]
	float cam_fpx = 0;					// focal length [pix]

	Mat2f R_BC = Mat2f::Identity();		// camera to body transform

	ControlVar posx_control;
	ControlVar posy_control;
	ControlVar yaw_control;
	ControlVar throttle_control;

	ControlVar velx_control;
	ControlVar vely_control;
	ControlVar yawrate_control;
	ControlVar vertical_control;

	float z_speed = 1;					// [scale / sec]
	float yaw_rate = 0;					// [deg / sec]
	Vec2f xy_speed = Vec2f(0, 0);		// body frame [pix / sec]

	Vec2f offset;						// body frame

	Transform2D odom;					// camera frame

	ControlOutput out;

protected:
	void init() override
	{
		posx_control.gain = position_gain;
		posy_control.gain = position_gain;
		yaw_control.gain = yaw_gain;
		throttle_control.gain = throttle_gain;

		posx_control.set_limit(-max_angle, max_angle);
		posy_control.set_limit(-max_angle, max_angle);
		yaw_control.set_limit(-max_yaw, max_yaw);
		throttle_control.set_limit(0, max_throttle / 1000.f);

		velx_control.gain = velocity_gain;
		vely_control.gain = velocity_gain;
		yawrate_control.gain = yawrate_gain;
		vertical_control.gain = vertical_gain;

		velx_control.set_limit(-max_angle, max_angle);
		vely_control.set_limit(-max_angle, max_angle);
		yawrate_control.set_limit(-max_yaw, max_yaw);
		vertical_control.set_limit(0, max_throttle / 1000.f);

		add_output("control", &out);
	}

	void exec() override
	{
		gyro = get_input<Gyro::State>("gyro");
		affine = get_input<Affine::Params>("affine");
		velocity = get_input<ImageVelocity>("affine_vel");
		cam_yaw = deg2rad(get_input<Float>("cam_yaw"));
		cam_fpx = get_input<Float>("cam_fpx");
		AGL = get_input<Float>("AGL");

		const int64_t ts = get_input<Integer64>("ts");

		dt = last_ts ? (ts - last_ts) * 1e-6 : 0;		// [sec]

		if(affine.valid()) {
			odom.add(affine.transform());
		}
		R_BC = get_rotation_matrix(cam_yaw);

		if(auto rc = get_input<ConstPointer>("msp_rc").get<MSP2::RcPacket>())
		{
			base_throttle = (float(rc->throttle()) - 1000) / 1000;

			const int override_index = (4 + override_channel) - 1;
			if(override_index < rc->ch.size()) {
				if(rc->ch.at(override_index) > override_threshold) {
					if(!active) {
						enable();
					}
				} else {
					if(active) {
						disable();
					}
				}
			}
			std::cout << "RC: roll = " << rc->roll() << ", pitch = " << rc->pitch() << ", yaw = " << rc->yaw() << ", throttle = " << rc->throttle() << std::endl;
		}

		// convert to body frame
		offset = R_BC * odom.pos;
		xy_speed = R_BC * velocity.xy;

		z_speed = velocity.z;
		yaw_rate = gyro.rates.z();

		std::cout << "Speed: xy = " << xy_speed.transpose() << " pix/s, yaw = " << yaw_rate << " deg/s, z = " << z_speed << std::endl;

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
			// fallback to hover when missing control input
			std::cout << "WARN: Missing control input, fallback to hover" << std::endl;

			exec_pos(PositionControl());
		}

		last_ts = ts;
	}

	void exec_vel(const VelocityControl& cmd)
	{
		if(mode != VEL) {
			mode = VEL;
			reset();
			std::cout << "INFO: Switched to VELOCITY control mode" << std::endl;
		}
		// convert target to image units
		const float factor = cam_fpx / std::max(AGL, AGL_min);

		const Vec2f target_vel = factor * Vec2f(cmd.vel.x(), cmd.vel.y());		// [pix/s]

		const float target_z = 1 + cmd.vel.z() / std::max(AGL, AGL_min);

		std::cout << "Target: xy = " << target_vel.transpose() << " pix/s, z = " << target_z << ", AGL = " << AGL << " m" << std::endl;

		if(active) {
			out.angle.x() = -1 * velx_control.update(target_vel.x(), xy_speed.x(), dt);
			out.angle.y() = -1 * vely_control.update(target_vel.y(), xy_speed.y(), dt);

			out.yaw_rate = -1 * yawrate_control.update(cmd.yaw_rate, yaw_rate, dt);

			out.throttle = vertical_control.update(target_z, z_speed, dt);
		}
		else {
			out.yaw_rate = 0;
			out.throttle = base_throttle;
			out.angle = Vec2f(0, 0);
		}

		send();
	}

	void exec_pos(const PositionControl& cmd)
	{
		if(mode != POS) {
			mode = POS;
			reset();
			std::cout << "INFO: Switched to POSITION control mode" << std::endl;
		}
		const float yaw_deg = angle_norm_180(gyro.get_rpy().z() - base_yaw);	// TODO: correct via odom

		std::cout << "Odometry: pos = " << offset.transpose() << ", yaw = " << yaw_deg << " deg, scale = " << odom.scale << std::endl;

		// convert target to image units
		const float factor = cam_fpx / std::max(AGL, AGL_min);

		const Vec2f target_pos = factor * Vec2f(cmd.pos.x(), cmd.pos.y());

		const float target_z = 1 + (cmd.pos.z() / std::max(base_AGL, AGL_min));

		const float target_yaw = angle_norm_180(cmd.yaw_deg);		// [deg]

		std::cout << "Target: xy = " << target_pos.transpose() << " pix, z = " << target_z << ", AGL = " << AGL << " m" << std::endl;

		if(active) {
			out.angle.x() = -1 * posx_control.update(target_pos.x(), offset.x(), dt);
			out.angle.y() = -1 * posy_control.update(target_pos.y(), offset.y(), dt);

			out.yaw_rate = -1 * yaw_control.update(target_yaw, yaw_deg, dt);

			out.throttle = throttle_control.update(target_z, odom.scale, dt);
		}
		else {
			out.yaw_rate = 0;
			out.throttle = base_throttle;
			out.angle = Vec2f(0, 0);
		}

		send();
	}

	void send()
	{
		const Vec3f RPY = gyro.get_rpy();		// [deg]

		// compensate for thrust vector loss
		const auto extra_throttle = 1 / (cosf(deg2rad(RPY.x())) * cosf(deg2rad(RPY.y())));
		out.throttle *= extra_throttle;

		out.throttle = std::min(std::max(out.throttle, 0.f), 1.f);

		if(active) {
			response_xy.add_sample(out.angle, xy_speed);

			const Mat2f A = response_xy.get_matrix_or_identity();
			std::cout << "Response: angle = " << rad2deg(response_xy.get_rotation()) << " deg, [" << A.col(0).transpose() << "] [" << A.col(1).transpose() << "]" << std::endl;

			std::cout << "Control: roll = " << out.angle.x() << ", pitch = " << out.angle.y() << ", yaw = " << out.yaw_rate
					<< ", throttle = " << out.throttle << " (extra " << extra_throttle << ")" << std::endl;
		}

		std::array<uint16_t, 8> rc = {};
		rc[0] = 1500 + std::min(std::max(int(out.angle.x()), -max_angle), max_angle);	// roll
		rc[1] = 1500 + std::min(std::max(int(out.angle.y()), -max_angle), max_angle);	// pitch
		rc[2] = 1000 + std::min(std::max(int(out.throttle * 1000), 0), max_throttle);	// throttle
		rc[3] = 1500 + std::min(std::max(int(out.yaw_rate), -max_yaw), max_yaw);		// yawrate

		std::cout << "RC_OVERRIDE: " << to_string(rc) << std::endl;

		if(msp) {
			msp->send_raw_rc(rc);
		}
	}

	void enable()
	{
		active = true;

		reset();

		std::cout << "Control: Enabled with yaw " << base_yaw << " deg" << std::endl;
	}

	void disable()
	{
		active = false;

		std::cout << "Control: Disabled" << std::endl;
	}

	void reset()
	{
		// reset odometry
		odom = Transform2D();
		offset = Vec2f::Zero();

		// reset controllers
		yaw_control.reset(0);
		posx_control.reset(0);
		posy_control.reset(0);
		throttle_control.reset(base_throttle);

		velx_control.reset(0);
		vely_control.reset(0);
		yawrate_control.reset(0);
		vertical_control.reset(base_throttle);

		// reset base values (for position mode)
		base_AGL = std::max(AGL, AGL_min);
		base_yaw = gyro.get_rpy().z();
	}

private:
	enum mode_e {
		NONE, VEL, POS
	} mode = NONE;

	bool active = false;

	float base_AGL = 0;				// [m]
	float base_yaw = 0;				// [deg]
	float base_throttle = 0.5;

	int64_t last_ts = 0;			// [us]

	ResponseEstimator2D response_xy;

	MSP2* msp = nullptr;

};





} // mmpilot

#endif /* INCLUDE_MMPILOT_CONTROL_STAGE_H_ */
