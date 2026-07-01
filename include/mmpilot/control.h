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


} // mmpilot

#endif /* INCLUDE_MMPILOT_CONTROL_H_ */
