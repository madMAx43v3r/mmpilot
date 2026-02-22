/*
 * calib.h
 *
 *  Created on: Feb 20, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_CALIB_H_
#define INCLUDE_MMPILOT_CALIB_H_

#include <mmpilot/math.h>

#include <Eigen/Core>
#include <Eigen/Geometry>

#include <vector>


namespace mmpilot {
namespace calib {

struct ExtrinsicSample {
	Mat3f dR_B = Mat3f::Identity();		// delta rotation matrix
	Vec2f trans = Vec2f::Zero();		// pixels
	float alpha = 0;					// rad
};


// One batch GN/LS step using translation+inplane rotation residuals from affine homography
// Inputs per pair k: (tx, ty) in pixels, alpha in radians, dR_B from gyro.
// fx, fy are virtual-view focal lengths in pixels.
inline Eigen::Vector3d solve_delta_extrinsic(
    const Eigen::Matrix3d& R_BC,
    const std::vector<ExtrinsicSample>& samples,
    double fx, double fy,
    double lambda = 1e-6
){
    Eigen::Matrix3d H = Eigen::Matrix3d::Zero();
    Eigen::Vector3d g = Eigen::Vector3d::Zero();

    for(const auto& sample : samples)
    {
        const Eigen::Matrix3d& dR_B = sample.dR_B.cast<double>();
        const double tx = sample.trans.x();
        const double ty = sample.trans.y();
        const double a  = sample.alpha;

        // measurement: omega_meas ≈ [ty/fy, -tx/fx, -alpha]
        Eigen::Vector3d omega_meas;
        omega_meas << (ty / fy),
                      (-tx / fx),
                      (-a); // flip sign here if your convention differs

        // C = R dR_B R^T
        const Eigen::Matrix3d C = R_BC * dR_B * R_BC.transpose();

        // M = C^T - I  (analytic Jacobian: omega_pred = M * delta)
        const Eigen::Matrix3d M = C.transpose() - Eigen::Matrix3d::Identity();

        H += (M.transpose() * M);
        g += (M.transpose() * omega_meas);
    }

    H += lambda * Eigen::Matrix3d::Identity();

    return H.ldlt().solve(g); // delta that best explains measured residuals
}

// Apply update: R <- exp(delta) R
inline void apply_delta_rot(Eigen::Matrix3d& R_BC, const Eigen::Vector3d& delta)
{
    R_BC = so3_exp(delta) * R_BC;
}


} // calib
} // mmpilot

#endif /* INCLUDE_MMPILOT_CALIB_H_ */
