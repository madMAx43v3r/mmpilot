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

#include <mutex>
#include <thread>
#include <algorithm>
#include <condition_variable>


namespace mmpilot {

class ControlThread {
public:
	int interval_us = 20 * 1000;		// [usec]

	ControlThread(MSP2* msp_) : msp(msp_) {}

	~ControlThread()
	{
		stop();
		join();
	}

	void start()
	{
		if(!do_run) {
			std::unique_lock<std::mutex> lock(mutex);
			do_run = true;
			thread = std::thread(&ControlThread::run, this);
		}
	}

	void stop()
	{
		std::lock_guard<std::mutex> lock(mutex);
		do_run = false;
		signal.notify_all();
	}

	void join()
	{
		if(thread.joinable()) {
			thread.join();
		}
	}

	void update(std::shared_ptr<const ControlOutput> cmd)
	{
		std::lock_guard<std::mutex> lock(mutex);
		prev = next;
		next = cmd;
	}

protected:
	void run()
	{
		while(true) {
			std::unique_lock<std::mutex> lock(mutex);
			if(!do_run) {
				break;
			}

			if(prev && next && next->ts > prev->ts)
			{
				const int64_t dt = next->ts - prev->ts;
				const int64_t ts = std::min(std::max(get_time_micros() - dt, prev->ts), next->ts);

				const float t = float(ts - prev->ts) / dt;

				ControlOutput cmd;
				cmd.ts = ts;
				cmd.angle    = prev->angle * (1 - t) + next->angle * t;
				cmd.yaw_rate = prev->yaw_rate * (1 - t) + next->yaw_rate * t;
				cmd.throttle = next->throttle;
				send(cmd);
			}
			signal.wait_for(lock, std::chrono::microseconds(interval_us));
		}
	}

	void send(const ControlOutput& cmd)
	{
		const auto now = get_time_micros();

		if(now - last_send < interval_us - 100) {
			return;
		}
		std::array<uint16_t, 8> rc = {};
		rc[0] = 1500 + std::clamp<int>(std::lround(cmd.angle.x()), -500, 500);		// roll
		rc[1] = 1500 + std::clamp<int>(std::lround(cmd.angle.y()), -500, 500);		// pitch
		rc[2] = 1000 + std::clamp<int>(std::lround(cmd.throttle), 0, 1000);			// throttle
		rc[3] = 1500 + std::clamp<int>(std::lround(cmd.yaw_rate), -500, 500);		// yawrate

		if(msp) {
			msp->send_raw_rc(rc);
		}
		last_send = now;
	}

private:
	std::mutex mutex;
	std::thread thread;
	std::condition_variable signal;

	bool do_run = false;
	int64_t last_send = 0;
	std::shared_ptr<const ControlOutput> prev;
	std::shared_ptr<const ControlOutput> next;

	MSP2* msp = nullptr;

};


class ControlStage : public Stage {
public:
	float max_yaw = 50;					// RC offset
	float max_angle = 200;				// RC offset
	float max_throttle = 700;			// RC offset

	float max_yaw_rate = 50;			// RC / sec
	float max_angle_rate = 200;			// RC / sec
	float max_throttle_rate = 1000;		// RC / sec

	float yaw_gain = 1;
	float position_gain = 2;
	float throttle_gain = 0.2;

	float yawrate_gain = 0.5;
	float velocity_gain = 1;
	float vertical_gain = 0.1;

	float AGL_min = 1;					// sanity limit [m]

	int override_channel = 5;			// AUX
	int override_threshold = 1550;

	ControlThread thread;


	ControlStage(MSP2* msp_)
		:	Stage("control"), thread(msp_)
	{
	}

	Gyro::State gyro;
	Affine::Params affine;
	ImageVelocity velocity;

	float dt = 0;						// [sec]
	float AGL = 0;						// [m]
	float cam_yaw = 0;					// [rad]
	float cam_fpx = 0;					// focal length [pix]

	ControlVar posx_control;
	ControlVar posy_control;
	ControlVar yaw_control;
	ControlVar throttle_control;

	ControlVar velx_control;
	ControlVar vely_control;
	ControlVar yawrate_control;
	ControlVar vertical_control;

	float yaw_rate = 0;					// [deg / sec]

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

		posx_control.set_limit(-max_angle, max_angle, max_angle_rate);
		posy_control.set_limit(-max_angle, max_angle, max_angle_rate);
		yaw_control.set_limit(-max_yaw, max_yaw, max_yaw_rate);
		throttle_control.set_limit(0, max_throttle / 1000.f, max_throttle_rate / 1000.f);

		velx_control.gain = velocity_gain;
		vely_control.gain = velocity_gain;
		yawrate_control.gain = yawrate_gain;
		vertical_control.gain = vertical_gain;

		velx_control.set_limit(-max_angle, max_angle, max_angle_rate);
		vely_control.set_limit(-max_angle, max_angle, max_angle_rate);
		yawrate_control.set_limit(-max_yaw, max_yaw, max_yaw_rate);
		vertical_control.set_limit(0, max_throttle / 1000.f, max_throttle_rate / 1000.f);

		thread.start();

		add_output("control", &out);
	}

	void exec() override
	{
		dt = get_input<Float>("dt");
		gyro = get_input<Gyro::State>("gyro");
		affine = get_input<Affine::Params>("affine");
		velocity = get_input<ImageVelocity>("affine_vel");
		cam_yaw = deg2rad(get_input<Float>("cam_yaw"));
		cam_fpx = get_input<Float>("cam_fpx");
		AGL = get_input<Float>("AGL");

		if(affine.valid()) {
			odom.add(affine.transform());
		}

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

		yaw_rate = gyro.rates.z();

		std::cout << "Speed: xy = " << velocity.xy.transpose() << " pix/s, yaw = " << yaw_rate << " deg/s, z = " << velocity.z << std::endl;

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

		const float target_z = cmd.vel.z() / std::max(AGL, AGL_min);

		std::cout << "Target: xy = " << target_vel.transpose() << " pix/s, z = " << target_z << ", AGL = " << AGL << " m" << std::endl;

		if(active) {
			out.angle.x() = 1 * velx_control.update(target_vel.x(), velocity.xy.x(), dt);
			out.angle.y() = 1 * vely_control.update(target_vel.y(), velocity.xy.y(), dt);

			out.yaw_rate = -1 * yawrate_control.update(cmd.yaw_rate, yaw_rate, dt);

			out.throttle = vertical_control.update(target_z, velocity.z, dt);
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
		// convert odometry to body frame
		offset = get_rotation_matrix(-cam_yaw) * odom.pos;

		const float yaw_deg = angle_norm_180(gyro.yaw() - base_yaw);	// TODO: correct via odom

		std::cout << "Odometry: pos = " << offset.transpose() << ", yaw = " << yaw_deg << " deg, scale = " << odom.scale << std::endl;

		// convert target to image units
		const float factor = cam_fpx / std::max(AGL, AGL_min);

		const Vec2f target_pos = factor * Vec2f(cmd.pos.x(), cmd.pos.y());

		const float target_z = 1 + (cmd.pos.z() / std::max(base_AGL, AGL_min));

		const float target_yaw = angle_norm_180(cmd.yaw_deg);		// [deg]

		std::cout << "Target: xy = " << target_pos.transpose() << " pix, z = " << target_z << ", AGL = " << AGL << " m" << std::endl;

		if(active) {
			out.angle.x() = 1 * posx_control.update(target_pos.x(), offset.x(), dt);
			out.angle.y() = 1 * posy_control.update(target_pos.y(), offset.y(), dt);

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
		const Vec3f RPY = gyro.RPY();		// [deg]

		// compensate for thrust vector loss
		const auto extra_throttle = 1 / (cosf(deg2rad(RPY.x())) * cosf(deg2rad(RPY.y())));
		out.throttle *= extra_throttle;

		out.throttle = std::min(std::max(out.throttle, 0.f), 1.f);

		if(active) {
			response_xy.add_sample(out.angle, velocity.xy);

			const Mat2f A = response_xy.get_matrix_or_identity();
			std::cout << "Response: angle = " << rad2deg(response_xy.get_rotation()) << " deg, [" << A.col(0).transpose() << "] [" << A.col(1).transpose() << "]" << std::endl;

			std::cout << "Control: roll = " << out.angle.x() << ", pitch = " << out.angle.y() << ", yaw = " << out.yaw_rate
					<< ", throttle = " << out.throttle << " (extra " << extra_throttle << ")" << std::endl;
		}

		auto cmd = std::make_shared<ControlOutput>();
		cmd->ts = get_time_micros();
		cmd->angle.x() = std::clamp<float>(out.angle.x(), -max_angle, max_angle);		// roll
		cmd->angle.y() = std::clamp<float>(out.angle.y(), -max_angle, max_angle);		// pitch
		cmd->throttle  = std::clamp<float>(out.throttle * 1000, 0.f, max_throttle);		// throttle
		cmd->yaw_rate  = std::clamp<float>(out.yaw_rate, -max_yaw, max_yaw);			// yawrate

		thread.update(cmd);
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
		base_yaw = gyro.yaw();
	}

private:
	enum mode_e {
		NONE, VEL, POS
	} mode = NONE;

	bool active = false;

	float base_AGL = 0;				// [m]
	float base_yaw = 0;				// [deg]
	float base_throttle = 0.5;

	ResponseEstimator2D response_xy;

};





} // mmpilot

#endif /* INCLUDE_MMPILOT_CONTROL_STAGE_H_ */
