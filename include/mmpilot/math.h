/*
 * math.h
 *
 *  Created on: Feb 16, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_MATH_H_
#define INCLUDE_MMPILOT_MATH_H_

#include <Eigen/Dense>


namespace mmpilot {

using Vec2f = Eigen::Matrix<float, 2, 1>;
using Vec2d = Eigen::Matrix<double, 2, 1>;

using Mat3f = Eigen::Matrix<float, 3, 3>;
using Mat3d = Eigen::Matrix<double, 3, 3>;

using Vec3f = Eigen::Matrix<float, 3, 1>;
using Vec3d = Eigen::Matrix<double, 3, 1>;



} // mmpilot

#endif /* INCLUDE_MMPILOT_MATH_H_ */
