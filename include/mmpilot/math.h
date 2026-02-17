/*
 * math.h
 *
 *  Created on: Feb 16, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_MATH_H_
#define INCLUDE_MMPILOT_MATH_H_

#include <Eigen/Dense>

#include <cmath>


namespace mmpilot {

using Vec2f = Eigen::Matrix<float, 2, 1>;
using Vec2d = Eigen::Matrix<double, 2, 1>;

using Mat2f = Eigen::Matrix<float, 2, 2>;
using Mat2d = Eigen::Matrix<double, 2, 2>;

using Mat3f = Eigen::Matrix<float, 3, 3>;
using Mat3d = Eigen::Matrix<double, 3, 3>;

using Vec3f = Eigen::Matrix<float, 3, 1>;
using Vec3d = Eigen::Matrix<double, 3, 1>;


template<typename T>
T deg2rad(T d) {
	return d * T(M_PI / 180);
}

template<typename T>
T rad2deg(T r) {
	return r * T(180 / M_PI);
}

// Wrap to [-180, 180)
template<typename T>
T angle_norm_180(T deg)
{
	// atan2(sin, cos) gives [-pi, pi]
	const T r = std::atan2(std::sin(deg2rad(deg)), std::cos(deg2rad(deg)));
	const T d = rad2deg(r); // [-180, 180]
	// make it [-180,180)
	return d < 180 ? d : d - 360;
}

// Wrap to [0, 360)
template<typename T>
T angle_norm_360(T deg)
{
	const T d = std::fmod(deg, T(360));
	return d >= 0 ? d : d + 360;
}

// Smallest signed delta to go from a -> b in degrees, result in [-180, 180)
template<typename T>
T angle_delta_180(T a_deg, T b_deg)
{
	return angle_norm_180(b_deg - a_deg);
}

template<typename T>
T get_angle(const Eigen::Matrix<T,2,2>& R)
{
	return std::atan2(R(1,0), R(0,0));
}

template<typename T>
Eigen::Matrix<T,2,2> normalize_rot(Eigen::Matrix<T,2,2>& R)
{
	const auto a = get_angle(R);
	Eigen::Matrix<T,2,2> Q;
	Q << std::cos(a), -std::sin(a),
		 std::sin(a),  std::cos(a);
	return Q;
}

// Rotation matrix from roll/pitch/yaw in degrees, ZYX order:
// R = Rz(yaw) * Ry(pitch) * Rx(roll)
template<typename T>
Eigen::Matrix<T,3,3> rpy_to_rot_zyx_deg(const Eigen::Matrix<T,3,1>& rpy_deg)
{
	const float cr = std::cos(deg2rad(rpy_deg[0]));
	const float sr = std::sin(deg2rad(rpy_deg[0]));
	const float cp = std::cos(deg2rad(rpy_deg[1]));
	const float sp = std::sin(deg2rad(rpy_deg[1]));
	const float cy = std::cos(deg2rad(rpy_deg[2]));
	const float sy = std::sin(deg2rad(rpy_deg[2]));

	Eigen::Matrix<T,3,3> R;
	// ZYX (yaw-pitch-roll)
	R(0, 0) = cy * cp;
	R(0, 1) = cy * sp * sr - sy * cr;
	R(0, 2) = cy * sp * cr + sy * sr;

	R(1, 0) = sy * cp;
	R(1, 1) = sy * sp * sr + cy * cr;
	R(1, 2) = sy * sp * cr - cy * sr;

	R(2, 0) = -sp;
	R(2, 1) = cp * sr;
	R(2, 2) = cp * cr;

	return R;
}


} // mmpilot

#endif /* INCLUDE_MMPILOT_MATH_H_ */
