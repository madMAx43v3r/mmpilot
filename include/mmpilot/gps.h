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
#include <mmpilot/value.h>
#include <mmpilot/wgs84.h>

#include <list>
#include <memory>
#include <cstdint>
#include <stdexcept>
#include <iostream>


namespace mmpilot {

class GPS {
public:
	class State : public Value {
	public:
		int64_t ts = 0;			// [us]

		int fix_type = 0;		// 0 = no fix, 1 = 2D, 2 = 3D
		int num_sats = 0;

		double lat_deg = 0;			// [deg]
		double lon_deg = 0;			// [deg]
		double alt_m = 0;			// [m]

		double speed_ms = 0;		// [m/s]
		double heading_deg = 0;		// [deg]

		State extrapolate(int64_t delta_us) const
		{
			State out = *this;
			out.ts += delta_us;

			if(fix_type <= 0) {
				return out;
			}
			const double dt = double(delta_us) * 1e-6;
			const double dist = speed_ms * dt;
			const double yaw = deg2rad(heading_deg);

			const WGS84<double> wgs(deg2rad(lat_deg), deg2rad(lon_deg), alt_m);
			const auto ll = wgs.get_ll(sin(yaw) * dist, cos(yaw) * dist);

			out.lat_deg = rad2deg(ll.x());
			out.lon_deg = rad2deg(ll.y());
			return out;
		}

		std::string to_string() const override {
			return mmpilot::to_string(std::array<double, 3>{lat_deg, lon_deg, alt_m});
		}
	};

	size_t max_history = 1000;					// samples

	int64_t max_extrapolate_ms = 1000;

	void on_gps(const MSP2::RawGPS& gps)
	{
		if(gps.fix_type <= 0) {
			return;
		}
		if(gps.ts <= head_ts()) {
			return;
		}
		State s;
		s.ts = gps.ts;
		s.fix_type = gps.fix_type;
		s.num_sats = gps.num_sats;

		s.lat_deg = gps.get_lat();
		s.lon_deg = gps.get_lon();
		s.alt_m = gps.get_alt();

		s.speed_ms = gps.get_speed();
		s.heading_deg = angle_norm_360(gps.get_heading());

		history.push_back(s);

		while(history.size() > max_history) {
			history.pop_front();
		}
	}

	std::shared_ptr<const State> lookup(const int64_t ts, const bool strict = true) const
	{
		if(history.empty()) {
			return nullptr;
		}
		if(ts < front_ts()) {
			if(strict) {
				return nullptr;
			}
			return std::make_shared<State>(history.front());
		}
		if(ts > head_ts()) {
			if(strict) {
				return nullptr;
			}
			const auto& state = history.back();
			return std::make_shared<State>(
					state.extrapolate(std::min(ts - state.ts, max_extrapolate_ms * 1000)));
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

		auto out = std::make_shared<State>();
		out->ts = ts;
		out->lat_deg = lerp(a.lat_deg, b.lat_deg, t);
		out->lon_deg = lerp(a.lon_deg, b.lon_deg, t);
		out->alt_m = lerp(a.alt_m, b.alt_m, t);
		out->speed_ms = lerp(a.speed_ms, b.speed_ms, t);
		out->heading_deg = lerp_deg(a.heading_deg, b.heading_deg, t);

		if(t < 0.5) {
			out->num_sats = a.num_sats;
			out->fix_type = a.fix_type;
		} else {
			out->num_sats = b.num_sats;
			out->fix_type = b.fix_type;
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
