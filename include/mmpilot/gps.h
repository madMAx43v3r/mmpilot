/*
 * gps.h
 *
 *  Created on: Feb 26, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_GPS_H_
#define INCLUDE_MMPILOT_GPS_H_

#include <mmpilot/beta_msp.h>
#include <mmpilot/math.h>

#include <list>
#include <cstdint>
#include <stdexcept>
#include <iostream>


namespace mmpilot {

class GPS {
public:
	class State {
	public:
		int64_t ts = 0;			// [us]

		int fix_type = 0;		// 0 = no fix, 2 = 2D, 3 = 3D
		int num_sats = 0;

		double lat = 0;			// [deg]
		double lon = 0;			// [deg]
		double alt = 0;			// [m]

		double speed = 0;		// [m/s]
		double heading = 0;		// [deg]
	};

	size_t max_history = 1000;					// samples

	void on_gps(const MSP2Client::RawGPS& gps)
	{
		if(gps.ts <= head_ts()) {
			return;
		}
		State s;
		s.ts = gps.ts;
		s.fix_type = gps.fix_type;
		s.num_sats = gps.num_sats;

		s.lat = gps.lat * 1e-7;
		s.lon = gps.lon * 1e-7;
		s.alt = gps.alt * 0.01;

		s.speed = gps.speed * 0.01;
		s.heading = gps.course * 0.1;
		s.heading = angle_norm_360(s.heading);

		history.push_back(s);

		while(history.size() > max_history) {
			history.pop_front();
		}
	}

	State lookup(const int64_t ts) const
	{
		if(history.empty()) {
			throw std::runtime_error("GPS::lookup(): history is empty");
		}
		if(ts < front_ts()) {
			std::cout << "GPS::lookup(): requested ts beyond history: " << ts << std::endl;
			return history.front();
		}
		if(ts > head_ts()) {
			std::cout << "GPS::lookup(): requested ts in future by " << (ts - head_ts()) << " us" << std::endl;
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
			throw std::logic_error("GPS::lookup() error");
		}
		const State& a = *it_a;
		const State& b = *it_b;

		const double dt = b.ts - a.ts;
		const double t = (dt > 0) ? ((ts - a.ts) / dt) : 0;

		State out;
		out.ts = ts;
		out.lat = lerp(a.lat, b.lat, t);
		out.lon = lerp(a.lon, b.lon, t);
		out.alt = lerp(a.alt, b.alt, t);
		out.speed = lerp(a.speed, b.speed, t);
		out.heading = lerp_deg(a.heading, b.heading, t);

		// For discrete fields, pick more recent (or choose based on t)
		if(t < 0.5) {
			out.num_sats = a.num_sats;
			out.fix_type = a.fix_type;
		} else {
			out.num_sats = b.num_sats;
			out.fix_type = b.fix_type;
		}
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

	static double lerp(double a, double b, double t) {
		return a + (b - a) * t;
	}

	static double lerp_deg(double a, double b, double t)
	{
		const auto d = angle_norm_180(b - a);
		return angle_norm_360(a + d * t);
	}

};


} // mmpilot

#endif /* INCLUDE_MMPILOT_GPS_H_ */
