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
#include <iostream>


namespace mmpilot {

class Gyro {
public:
	struct State {
		int64_t ts = 0;				// [us]
		Vec3f RPY = Vec3f::Zero();	// [deg]

		Mat3f matrix() const {
			return rpy_to_rot_zyx_deg<Mat3f>(RPY);
		}
	};

	size_t max_history = 10000;		// samples

	float imu_scale = 1 / 16.4;				// [deg/s]

	Vec3f att_scale = Vec3f(0.1, 0.1, 1);	// [deg]

	void on_raw_imu(const MSP2Client::RawImu& imu)
	{
		if(history.empty() || imu.ts <= history.back().ts) {
			return;
		}
		const auto prev = history.back();
		const auto dt = (imu.ts - prev.ts) * 1e-6f;		// [sec]
		const Vec3f rates = Vec3f(imu.gyro[0], imu.gyro[1], -imu.gyro[2]) * imu_scale;	// [deg/s]

		State out;
		out.ts = imu.ts;
		out.RPY = Vec3f(
			angle_norm_180(prev.RPY[0] + dt * rates[0]),
			angle_norm_180(prev.RPY[1] + dt * rates[1]),
			angle_norm_360(prev.RPY[2] + dt * rates[2])
		);
		history.push_back(out);

//		std::cout << "gyro imu raw: ts=" << out.ts << " roll=" << imu.gyro[0] << " pitch=" << imu.gyro[1] << " yaw=" << imu.gyro[2] << std::endl;
//		std::cout << "gyro imu rates: ts=" << out.ts << " roll=" << rates[0] << " pitch=" << rates[1] << " yaw=" << rates[2] << std::endl;
//		std::cout << "gyro imu: ts=" << out.ts << " roll=" << out.RPY[0] << " pitch=" << out.RPY[1] << " yaw=" << out.RPY[2] << std::endl;

		while(history.size() > max_history) {
			history.pop_front();
		}
	}

	void on_attitude(const MSP2Client::Attitude& att)
	{
		if(!history.empty() && att.ts <= history.back().ts) {
			return;
		}
		State out;
		out.ts = att.ts;
		out.RPY = Vec3f(att.roll, att.pitch, att.yaw).cwiseProduct(att_scale);
		out.RPY[0] = angle_norm_180(out.RPY[0]);
		out.RPY[1] = angle_norm_180(out.RPY[1]);
		out.RPY[2] = angle_norm_360(out.RPY[2]);
		history.push_back(out);

//		std::cout << "gyro att: ts=" << out.ts << " roll=" << out.RPY[0] << " pitch=" << out.RPY[1] << " yaw=" << out.RPY[2] << std::endl;

		while(history.size() > max_history) {
			history.pop_front();
		}
	}

	// R = Rz(yaw) * Ry(pitch) * Rx(roll)
	Mat3f matrix(const int64_t ts) const
	{
		const auto s = lookup(ts);
		return rpy_to_rot_zyx_deg<Mat3f>(s.RPY);
	}

	State lookup(const int64_t ts) const
	{
		if(history.empty()) {
			throw std::runtime_error("Gyro::lookup(): history is empty");
		}

		// Clamp outside range
		if(ts <= history.front().ts) {
			return history.front();
		}
		if(ts >= history.back().ts) {
			return history.back();
		}

		// Find bracketing states [a,b] with a.ts <= ts <= b.ts
		auto itB = history.begin();
		auto itA = itB++;
		for(; itB != history.end(); ++itA, ++itB) {
			if(itB->ts >= ts) {
				break;
			}
		}
		// itB must be valid due to clamping above
		const State& a = *itA;
		const State& b = *itB;

		const int64_t dt_us = b.ts - a.ts;
		if(dt_us <= 0) {
			// should not happen if timestamps are strictly increasing
			return a;
		}
		const float t = float(ts - a.ts) / float(dt_us); // 0..1

		State out;
		out.ts = ts;

		// Roll/Pitch: interpolate using shortest delta in [-180,180)
		out.RPY[0] = angle_norm_180(a.RPY[0] + t * angle_delta_180(a.RPY[0], b.RPY[0]));
		out.RPY[1] = angle_norm_180(a.RPY[1] + t * angle_delta_180(a.RPY[1], b.RPY[1]));

		// Yaw: shortest delta in (-180,180], then wrap to [0,360)
		out.RPY[2] = angle_norm_360(a.RPY[2] + t * angle_delta_180(a.RPY[2], b.RPY[2]));

		return out;
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

};


} // mmpilot

#endif /* INCLUDE_MMPILOT_GYRO_H_ */
