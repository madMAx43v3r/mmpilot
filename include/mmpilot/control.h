/*
 * control.h
 *
 *  Created on: Jun 30, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_CONTROL_H_
#define INCLUDE_MMPILOT_CONTROL_H_

#include <mmpilot/value.h>
#include <mmpilot/math.h>
#include <mmpilot/util.h>


namespace mmpilot {

class Velocity : public Value {
public:
	Vec2f xy = Vec2f::Zero();		// body frame [m/s]

	float yaw_rate = 0;				// deg/s
	float z = 0;					// [m/s]

};

class ImageVelocity : public Value {
public:
	Vec2f xy = Vec2f::Zero();		// image frame [pix/s]

	float yaw_rate = 0;				// deg/s
	float z = 1;					// scale

};

class PositionControl : public Value {
public:
	Vec3f pos = Vec3f::Zero();		// relative [m]

	float yaw_deg = 0;				// [deg]

	std::string to_string() const override {
		return mmpilot::to_string(std::array<float, 4>{pos.x(), pos.y(), pos.z(), yaw_deg});
	}

};

class VelocityControl : public Value {
public:
	Vec3f vel = Vec3f::Zero();		// [m/s]

	float yaw_rate = 0;				// [deg/s]

	std::string to_string() const override {
		return mmpilot::to_string(std::array<float, 4>{vel.x(), vel.y(), vel.z(), yaw_rate});
	}

};

class ControlOutput : public Value {
public:
	Vec2f angle = Vec2f::Zero();	// ticks
	float throttle = 0;				// 0 to 1
	float yaw_rate = 0;				// ticks

	std::string to_string() const override {
		return mmpilot::to_string(std::array<float, 4>{angle.x(), angle.y(), throttle, yaw_rate});
	}

};

template<typename T>
class ControlPD {
public:
	Vec2f PD = Vec2f(1, -1);		// (P, D)

	float gain = 1;					// global gain

	ControlPD() = default;

	T update(const T& err, const T& rate)
	{
		return (err * PD.x() + rate * PD.y()) * gain;
	}
};

class ControlVar {
public:
	float gain = 1;					// global gain
	float rate_gain = 0.5;			// derivative gain
	float look_ahead = 1;			// [sec]

	float min_value = 0;
	float max_value = 0;

	ControlVar() = default;

	void reset(float init)
	{
		state = init;
		rate = 0;
		last = 0;
	}

	void set_limit(float min, float max)
	{
		min_value = min;
		max_value = max;
	}

	float update(float err, const float dt)
	{
		err *= gain;

		if(dt > 0) {
			rate = exp_gain(rate, (err - last) / dt, rate_gain);
		}
		last = err;

		if(rate < 0) {
			err += rate * look_ahead;
		}
		state += err * dt;

		state = std::min(std::max(state, min_value), max_value);
		return state;
	}

	float state = 0;
	float rate = 0;

private:
	float last = 0;

};


} // mmpilot

#endif /* INCLUDE_MMPILOT_CONTROL_H_ */
