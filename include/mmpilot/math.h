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

using Vec3f = Eigen::Matrix<float, 3, 1>;
using Vec3d = Eigen::Matrix<double, 3, 1>;

using Mat2f = Eigen::Matrix<float, 2, 2>;
using Mat2d = Eigen::Matrix<double, 2, 2>;

using Mat3f = Eigen::Matrix<float, 3, 3>;
using Mat3d = Eigen::Matrix<double, 3, 3>;

template<typename T> using Vec2 = Eigen::Matrix<T, 2, 1>;
template<typename T> using Vec3 = Eigen::Matrix<T, 3, 1>;


template<typename T>
T deg2rad(T d) {
	return d * T(M_PI / 180);
}

template<typename T>
T rad2deg(T r) {
	return r * T(180 / M_PI);
}

// Wrap to [-pi, pi)
template<typename T>
T angle_norm_pi(T a) {
    return std::atan2(std::sin(a), std::cos(a));
}

// Wrap to [-180, 180)
template<typename T>
T angle_norm_180(T deg) {
	return rad2deg(angle_norm_pi(deg2rad(deg)));
}

// Wrap to [0, 360)
template<typename T>
T angle_norm_360(T deg) {
	const T d = std::fmod(deg, T(360));
	return d < 0 ? d + 360 : d;
}

// Returns to [-pi, pi)
template<typename T>
T get_angle(const Eigen::Matrix<T, 2, 2>& R) {
	return std::atan2(R(1,0), R(0,0));
}

// Returns to [-180, 180)
template<typename T>
T get_angle_deg(const Eigen::Matrix<T, 2, 2>& R) {
	return rad2deg(get_angle(R));
}

template<typename T>
Eigen::Matrix<T, 2, 2> get_rotation_matrix(const T theta)
{
	const T c = std::cos(theta);
	const T s = std::sin(theta);
	Eigen::Matrix<T, 2, 2> R;
	R << c, -s,
	     s,  c;
    return R;
}

template<typename T>
Eigen::Matrix<T, 2, 2> normalize_rot(Eigen::Matrix<T, 2, 2>& R)
{
	const T a = get_angle(R);
	Eigen::Matrix<T, 2, 2> Q;
	Q << std::cos(a), -std::sin(a),
		 std::sin(a),  std::cos(a);
	return Q;
}

// ZYX angles from matrix
template<typename T>
Eigen::Matrix<T, 3, 1> rot_zyx_to_rpy(const Eigen::Matrix<T, 3, 3>& R)
{
	const T yaw   = std::atan2(R(1,0), R(0,0));
	const T pitch = std::asin(-R(2,0));
	const T roll  = std::atan2(R(2,1), R(2,2));
	return {roll, pitch, yaw};
}

template<typename T>
Eigen::Matrix<T, 3, 1> rot_zyx_to_rpy_deg(const Eigen::Matrix<T, 3, 3>& R) {
	return rot_zyx_to_rpy(R) * T(180 / M_PI);
}

// Rotation matrix from roll/pitch/yaw in degrees, ZYX order:
// R = Rz(yaw) * Ry(pitch) * Rx(roll)
template<typename T>
Eigen::Matrix<T, 3, 3> rpy_to_rot_zyx(const Eigen::Matrix<T, 3, 1>& rpy_rad)
{
	const T cr = std::cos(rpy_rad[0]);
	const T sr = std::sin(rpy_rad[0]);
	const T cp = std::cos(rpy_rad[1]);
	const T sp = std::sin(rpy_rad[1]);
	const T cy = std::cos(rpy_rad[2]);
	const T sy = std::sin(rpy_rad[2]);

	Eigen::Matrix<T, 3, 3> R;
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

// Rotation matrix from roll/pitch/yaw in degrees, ZYX order:
// R = Rz(yaw) * Ry(pitch) * Rx(roll)
template<typename T>
Eigen::Matrix<T, 3, 3> rpy_to_rot_zyx_deg(const Eigen::Matrix<T, 3, 1>& rpy_deg)
{
	return rpy_to_rot_zyx<T>({
		deg2rad(rpy_deg.x()),
		deg2rad(rpy_deg.y()),
		deg2rad(rpy_deg.z())
	});
}

template<typename T>
Eigen::Matrix<T, 3, 3> slerp_R(const Eigen::Matrix<T, 3, 3>& R_0, const Eigen::Matrix<T, 3, 3>& R_1, const T t)
{
	Eigen::Quaternion<T> qa(R_0);
	Eigen::Quaternion<T> qb(R_1);

	// Ensure shortest path (avoid q and -q taking the long way)
	if(qa.dot(qb) < 0) {
		qb.coeffs() *= -1;
	}
	return qa.slerp(t, qb).normalized().toRotationMatrix();
}

template<typename T>
Eigen::Matrix<T, 3, 3> skew(const Eigen::Matrix<T, 3, 1>& w)
{
	Eigen::Matrix<T, 3, 3> W;
	W << 	 T(0),  -w.z(),  w.y(),
			 w.z(),  T(0),  -w.x(),
			-w.y(),  w.x(),   T(0);
	return W;
}

// Exp([w]x) with w = axis * angle (radians)
template<typename T>
Eigen::Matrix<T, 3, 3> so3_exp(const Eigen::Matrix<T, 3, 1>& w)
{
	const T th = w.norm();
	const Eigen::Matrix<T, 3, 3> W = skew(w);

	if(th < T(1e-8)) {
		// I + [w]x is fine at tiny angles
		return Eigen::Matrix<T, 3, 3>::Identity() + W;
	}
	const T a = std::sin(th) / th;
	const T b = (T(1) - std::cos(th)) / (th * th);
	return Eigen::Matrix<T, 3, 3>::Identity() + a * W + b * (W * W);
}

// rotation matrix -> rotation vector (axis*angle), radians, in the same frame as R
template<typename T>
Eigen::Matrix<T, 3, 1> so3_log(const Eigen::Matrix<T, 3, 3>& R)
{
	const T cos_th = (R.trace() - T(1)) * T(0.5);
	const T c = std::min(T(1), std::max(T(-1), cos_th));
	const T th = std::acos(c);

	if(th < T(1e-8)) {
		return Eigen::Matrix<T, 3, 1>::Zero();
	}
	Eigen::Matrix<T, 3, 1> v;
	v << (R(2, 1) - R(1, 2)),
		 (R(0, 2) - R(2, 0)),
		 (R(1, 0) - R(0, 1));
	v *= T(0.5) / std::sin(th);
	return v * th;
}

template<typename T>
Eigen::Matrix<T, 3, 3> orthonormalize(const Eigen::Matrix<T, 3, 3>& R)
{
    Eigen::JacobiSVD<Eigen::Matrix<T, 3, 3>> svd(R, Eigen::ComputeFullU | Eigen::ComputeFullV);
    Eigen::Matrix<T, 3, 3> U = svd.matrixU();
    Eigen::Matrix<T, 3, 3> V = svd.matrixV();
    Eigen::Matrix<T, 3, 3> Rn = U * V.transpose();
    if(Rn.determinant() < 0) {
        U.col(2) *= -1;
        Rn = U * V.transpose();
    }
    return Rn;
}




} // mmpilot

#endif /* INCLUDE_MMPILOT_MATH_H_ */
