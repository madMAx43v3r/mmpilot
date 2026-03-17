/*
 * wgs84.h
 *
 *  Created on: Feb 28, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_WGS84_H_
#define INCLUDE_MMPILOT_WGS84_H_

#include <mmpilot/math.h>


namespace mmpilot {

// WGS84 scales: meters per radian for lon (East) and lat (North)
// kE = (N + h) * cos(lat)
// kN = (M + h)
template<typename T>
void wgs84_en_scale(const T lat_rad, const T alt_m, T& kE, T& kN)
{
	const T a = T(6378137.0);
	const T f = T(1.0 / 298.257223563);
	const T e2 = f * (T(2) - f);

	const T s = std::sin(lat_rad);
	const T c = std::cos(lat_rad);

	const T denom = std::sqrt(T(1) - e2 * s * s);

	const T N = a / denom;                                   // prime vertical
	const T M = a * (T(1) - e2) / (denom * denom * denom);   // meridional

	kE = (N + alt_m) * c;
	kN = (M + alt_m);
}

template<typename T>
Vec3<T> wgs84_to_ecef(const T lat_rad,  const T lon_rad, const T alt_m)
{
	const T a  = T(6378137.0);
	const T f  = T(1.0 / 298.257223563);
	const T e2 = f * (T(2) - f);

	const T s = std::sin(lat_rad);
	const T c = std::cos(lat_rad);
	const T sl = std::sin(lon_rad);
	const T cl = std::cos(lon_rad);

	const T N = a / std::sqrt(T(1) - e2 * s * s);

	Vec3<T> ecef;
	ecef.x() = (N + alt_m) * c * cl;
	ecef.y() = (N + alt_m) * c * sl;
	ecef.z() = (N * (T(1) - e2) + alt_m) * s;
	return ecef;
}

template<typename T>
Vec3<T> ecef_to_enu_delta(const Vec3<T>& d, const T lat0, const T lon0)
{
	const T sphi = std::sin(lat0);
	const T cphi = std::cos(lat0);
	const T sl   = std::sin(lon0);
	const T cl   = std::cos(lon0);

	Vec3<T> enu;
	enu.x() = (-sl) * d.x() + (cl) * d.y() + T(0) * d.z();                  // East
	enu.y() = (-sphi*cl) * d.x() + (-sphi*sl) * d.y() + ( cphi) * d.z();    // North
	enu.z() = ( cphi*cl) * d.x() + ( cphi*sl) * d.y() + ( sphi) * d.z();    // Up
	return enu;
}

template<typename T>
Vec3<T> enu_to_ecef_delta(const Vec3<T>& enu, const T lat0, const T lon0)
{
	const T sphi = std::sin(lat0);
	const T cphi = std::cos(lat0);
	const T sl   = std::sin(lon0);
	const T cl   = std::cos(lon0);

	Vec3<T> d;
	d.x() = (-sl) * enu.x() + (-sphi * cl) * enu.y() + (cphi * cl) * enu.z();
	d.y() = ( cl) * enu.x() + (-sphi * sl) * enu.y() + (cphi * sl) * enu.z();
	d.z() = T(0) * enu.x() + ( cphi)      * enu.y() + (sphi)      * enu.z();
	return d;
}

// Returns {lat, lon, alt}
template<typename T>
Vec3<T> ecef_to_wgs84(const Vec3<T>& ecef)
{
	const T a  = T(6378137.0);
	const T f  = T(1.0 / 298.257223563);
	const T e2 = f * (T(2) - f);
	const T b  = a * (T(1) - f);
	const T ep2 = (a * a - b * b) / (b * b);

	const T x = ecef.x();
	const T y = ecef.y();
	const T z = ecef.z();

	const T p = std::sqrt(x * x + y * y);
	const T lon = std::atan2(y, x);

	// Handle pole case explicitly
	if(p < T(1e-12)) {
		const T lat = (z >= T(0)) ? T(M_PI_2) : T(-M_PI_2);
		const T alt = std::abs(z) - b;
		return Vec3<T>(lat, lon, alt);
	}

	// Bowring-style closed form
	const T theta = std::atan2(z * a, p * b);
	const T st = std::sin(theta);
	const T ct = std::cos(theta);

	const T lat = std::atan2(z + ep2 * b * st * st * st,
							 p - e2  * a * ct * ct * ct);

	const T s = std::sin(lat);
	const T N = a / std::sqrt(T(1) - e2 * s * s);
	const T alt = p / std::cos(lat) - N;

	return Vec3<T>(lat, lon, alt);
}


template<typename T>
class WGS84 {
public:
	const T lat0;			// [rad]
	const T lon0;			// [rad]
	const T alt0;			// [m]
	const Vec3<T> ecef0;	// [m]

	WGS84(T lat_rad, T lon_rad, T alt_m = 0)
		: lat0(lat_rad), lon0(lon_rad), alt0(alt_m),
		  ecef0(wgs84_to_ecef<T>(lat0, lon0, alt0))
	{
	}

	// Returns East-North offset in meters
	Vec2<T> get_en(T lat_rad, T lon_rad) const
	{
		const Vec3<T> p = wgs84_to_ecef<T>(lat_rad, lon_rad, alt0);
		const Vec3<T> d = ecef_to_enu_delta<T>(p - ecef0, lat0, lon0);
		return Vec2<T>(d.x(), d.y());
	}

	// Returns lat + lon for given EN offset in meters
	Vec2<T> get_ll(T E_m, T N_m) const
	{
		const Vec3<T> enu(E_m, N_m, 0);
		const Vec3<T> d_ecef = enu_to_ecef_delta<T>(enu, lat0, lon0);
		const Vec3<T> p_ecef = ecef0 + d_ecef;
		const Vec3<T> ll = ecef_to_wgs84<T>(p_ecef);
		return Vec2<T>(ll.x(), ll.y());
	}
};




} // mmpilot

#endif /* INCLUDE_MMPILOT_WGS84_H_ */
