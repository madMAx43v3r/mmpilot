/*
 * gyro.h
 *
 *  Created on: Feb 16, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_GYRO_H_
#define INCLUDE_MMPILOT_GYRO_H_

#include <mmpilot/beta_msp.h>
#include <mmpilot/math.h>

#include <list>
#include <cstdint>
#include <stdexcept>
#include <iostream>


namespace mmpilot {

class Gyro {
public:
	class State {
	public:
		int64_t ts = 0;		// [us]

		Mat3f rot = Mat3f::Identity();

		Vec3f rates = Vec3f::Zero();		// (roll, pitch, yaw) [deg/s]

		Mat3f matrix() const {
			return rot;
		}

		// RPY in deg
		Vec3f get_rpy() const {
			const auto raw = rot_zyx_to_rpy_deg(rot);
			return Vec3f(raw.x(), raw.y(), angle_norm_360(raw.z()));
		}
	};

	size_t max_history = 1000;					// samples

	float gyro_scale = 1 / 16.4;				// [deg/s]
	float accel_scale = 1 / 2048.f;				// [g]

	float att_window = 300;		// how fast to converge towards attitude [sec]

	Vec3f att_scale = Vec3f(0.1, 0.1, 1);		// [deg]

	void on_raw_imu(const MSP2Client::RawImu& imu)
	{
		if(history.empty() || imu.ts <= head_ts()) {
			return;
		}
		const auto prev = history.back();
		const float dt = (imu.ts - prev.ts) * 1e-6f;		// [sec]

		const Vec3f rates = Vec3f(imu.gyro[0], imu.gyro[1], imu.gyro[2]) * gyro_scale;	// [deg/s]

		const Vec3f omega = deg2rad(Vec3f(rates + att_correction));   	// [rad/s]

		State out;
		out.ts = imu.ts;
		out.rot = prev.rot * so3_exp<float>(omega * dt);
		out.rot = orthonormalize(out.rot);
		out.rates = rates;
		history.push_back(out);

		const auto RPY = out.get_rpy();

//		std::cout << "gyro imu raw:   ts=" << out.ts << " roll=" << imu.gyro[0] << ", pitch=" << imu.gyro[1] << ", yaw=" << imu.gyro[2] << std::endl;
//		std::cout << "gyro imu rates: ts=" << out.ts << " roll=" << rates[0] << " deg/s, pitch=" << rates[1] << " deg/s, yaw=" << rates[2] << " deg/s" << std::endl;
//		std::cout << "gyro imu rpy:   ts=" << out.ts << " roll=" << RPY[0] << " deg, pitch=" << RPY[1] << " deg, yaw=" << RPY[2] << " deg" << std::endl;

		const Vec3f accel = Vec3f(imu.acc[0], imu.acc[1], imu.acc[2]) * accel_scale;

		if(accel.norm() > 0.1) {
			const Vec3f g = accel.normalized();

			const auto pitch = rad2deg(std::asin(-g.x()));
			const auto roll  = rad2deg(std::atan2(g.y(), g.z()));

			att_correction = Vec3f(
						angle_norm_180(roll - RPY.x()),
						angle_norm_180(pitch - RPY.y()), 0) / att_window;

//			std::cout << "gyro accel att: ts=" << out.ts << " roll=" << roll << " deg, pitch=" << pitch << " deg" << std::endl;
		}

		while(history.size() > max_history) {
			history.pop_front();
		}
	}

	void on_attitude(const MSP2Client::Attitude& att)
	{
		const Vec3f RPY = Vec3f(att.roll, att.pitch, att.yaw).cwiseProduct(att_scale);

		if(history.empty()) {
			State out;
			out.ts = att.ts;
			out.rot = rpy_to_rot_zyx_deg<float>({RPY.x(), RPY.y(), 0});	// ignore yaw
			history.push_back(out);
		} else {
//			const auto state = history.back().get_rpy();
//			att_correction = Vec3f(
//					angle_norm_180(RPY.x() - state.x()),
//					angle_norm_180(RPY.y() - state.y()), 0) / att_window;
		}

//		std::cout << "gyro att: ts=" << att.ts << " roll=" << RPY[0] << " deg, pitch=" << RPY[1] << " deg, yaw=" << RPY[2] << " deg" << std::endl;
//		std::cout << "gyro att correction: ts=" << att.ts << " roll=" << att_correction[0] << " deg/s, pitch=" << att_correction[1] << " deg/s" << std::endl;
	}

	Mat3f matrix(const int64_t ts) const
	{
		return lookup(ts).matrix();
	}

	State lookup(const int64_t ts) const
	{
		if(history.empty()) {
			throw std::runtime_error("Gyro::lookup(): history is empty");
		}
		if(ts < front_ts()) {
			std::cout << "Gyro::lookup(): requested ts beyond history: " << ts << std::endl;
			return history.front();
		}
		if(ts > head_ts()) {
			std::cout << "Gyro::lookup(): requested ts in future by " << (ts - head_ts()) << " us" << std::endl;
			return history.back();
		}

		// Find bracketing states [a,b] with a.ts <= ts <= b.ts
		auto it_b = history.begin();
		auto it_a = it_b++;
		for(; it_b != history.end(); ++it_a, ++it_b) {
			if(it_b->ts >= ts) {
				break;
			}
		}
		if(it_b == history.end()) {
			throw std::logic_error("Gyro::lookup() error");
		}
		const auto& a = *it_a;
		const auto& b = *it_b;

		if(b.ts < a.ts) {
			throw std::logic_error("Gyro::lookup(): b.ts < a.ts");
		}
		const auto t = float(ts - a.ts) / (b.ts - a.ts); 	// 0..1

		State out;
		out.ts = ts;
		out.rot = slerp_R(a.rot, b.rot, t);
		out.rates = a.rates * (1 - t) + b.rates * t;
		return out;
	}

	int64_t front_ts() const
	{
		if(history.empty()) {
			return 0;
		}
		return history.front().ts;
	}

	int64_t head_ts() const
	{
		if(history.empty()) {
			return 0;
		}
		return history.back().ts;
	}

	bool avail() const {
		return !history.empty();
	}

private:
	std::list<State> history;

	Vec3f att_correction = Vec3f::Zero();		// [deg/s]

};


} // mmpilot

#endif /* INCLUDE_MMPILOT_GYRO_H_ */
