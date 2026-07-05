/*
 * model.h
 *
 *  Created on: Jul 5, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_MODEL_H_
#define INCLUDE_MMPILOT_MODEL_H_

#include <mmpilot/gyro.h>


namespace mmpilot {

class Model {
public:
	float bias_gain = 0.02;			// [1/sec]
	float update_gain = 0.5;		// [1/sec]

	float affine_rate = 5;			// [deg/s]

	Vec3f velocity;					// [m/s] (z aligned to gravity)

	Vec3f accel_bias;				// [m/s^2]

	Vec3f error;					// [m/s]


	Model() {
		reset();
	}

	void reset()
	{
		error = Vec3f::Zero();
		velocity = Vec3f::Zero();
		accel_bias = Vec3f::Zero();
		update_factor = 0;
		last_yaw = 0;
		have_yaw = false;
	}

	void step(const Gyro::State& gyro, const float dt)
	{
		const Vec3f RPY = gyro.get_rpy();	// [deg]
		const Vec3f rates = gyro.rates;		// [deg/s]
		const Vec3f accel = gyro.accel;		// [g]

		const float roll  = deg2rad(RPY.x());
		const float pitch = deg2rad(RPY.y());
		const float yaw   = deg2rad(RPY.z());

		// rotate old velocity into new yaw frame
		if(have_yaw) {
			const float dyaw = angle_norm_pi(yaw - last_yaw);

			const Mat2f R_z = get_rotation_matrix(-dyaw);
			const Vec2f vel = R_z * Vec2f(velocity.x(), velocity.y());
			velocity.x() = vel.x();
			velocity.y() = vel.y();
		}
		last_yaw = yaw;
		have_yaw = true;

		// transform to level frame
		const Mat3f R_yx = rpy_to_rot_zyx(Vec3f(roll, pitch, 0));

		Vec3f acc = R_yx * (accel * g_const);	// [m/s^2]

		acc.z() -= g_const;		// remove gravity

		acc -= accel_bias;		// remove bias

		velocity += acc * dt;

		update_factor = std::exp(-powf(rates.norm() / affine_rate, 2.f));
	}

	void update(const Vec3f& affine_vel, const float dt)
	{
		const float gain = update_gain * update_factor;		// [1/sec]

		const float alpha = 1 - std::exp(-gain * dt);

		error = affine_vel - velocity;

		velocity += error * alpha;	// apply correction

		// estimate bias from persistent velocity error
		const float beta = 1 - std::exp(-bias_gain * update_factor * dt);

		accel_bias -= error * beta;
	}


private:
	bool have_yaw = false;

	float last_yaw = 0;				// [rad]
	float update_factor = 0;

	const float g_const = 9.8f;

};


} // mmpilot

#endif /* INCLUDE_MMPILOT_MODEL_H_ */
