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
	Vec2f xy = Vec2f::Zero();		// body frame [pix/s]

	float yaw_rate = 0;				// deg/s
	float z = 0;					// scale factor

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
	float gain = 1;					// accel
	float damping = 2;				// deccel
	float target_time = 3;			// [sec]
	float output_gain = 0.2;		// output smoothing
	float look_ahead = 0.5;			// [sec]

	float min_value = 0;
	float max_value = 0;
	float max_rate = 0;

	ControlVar() = default;

	void reset(const float init)
	{
		bias = init;
		state = init;
		have_init = false;
	}

	void set_limit(float min, float max, float rate)
	{
		min_value = min;
		max_value = max;
		max_rate = std::abs(rate);
	}

	float update(const float target, const float current, const float dt)
	{
		float err = target - current;
		const float vel = have_init && dt > 0 ? (current - last) / dt : 0;

		err -= vel * look_ahead;

		const float target_vel = err / target_time;
		const float target_acc = vel / target_time;

		float next = bias + (target_vel - vel) * gain - target_acc * gain * damping;

		// smooth
		next = exp_gain(state, next, output_gain);

		// rate limit
		next = std::min(std::max(next, state - max_rate * dt), state + max_rate * dt);

		// bounds limit
		state = std::min(std::max(next, min_value), max_value);

		// update bias
		bias = exp_gain(bias, state, dt / target_time);

		// to compute velocity
		last = current;

		have_init = true;

		return state;
	}

	float bias = 0;
	float state = 0;
	float last = 0;

private:
	bool have_init = false;

};


class ResponseEstimator2D {
public:
	ResponseEstimator2D() {
		reset();
	}

	void reset()
	{
		RU.setZero();
		UU.setZero();
		count = 0;
	}

	void add_sample(const Vec2f& input, const Vec2f& response)
	{
		RU += response * input.transpose();
		UU += input * input.transpose();
		count++;
	}

	bool get_matrix(Mat2f& A) const
	{
		if(count < 2) {
			return false;
		}

		const float det = UU.determinant();

		if(std::abs(det) < 1e-6f) {
			return false;
		}

		A = RU * UU.inverse();
		return true;
	}

	Mat2f get_matrix_or_identity() const
	{
		Mat2f A;
		if(get_matrix(A)) {
			return A;
		}
		return Mat2f::Identity();
	}

	float get_rotation() const
	{
		const Mat2f A = get_matrix_or_identity();
		return std::atan2(
				A(1, 0) - A(0, 1),
				A(0, 0) + A(1, 1));
	}

private:
	Mat2f RU = Mat2f::Zero();	// sum(response * input^T)
	Mat2f UU = Mat2f::Zero();	// sum(input * input^T)
	size_t count = 0;

};


} // mmpilot

#endif /* INCLUDE_MMPILOT_CONTROL_H_ */
