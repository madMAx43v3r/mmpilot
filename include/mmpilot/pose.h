/*
 * pose.h
 *
 *  Created on: Mar 9, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_POSE_H_
#define INCLUDE_MMPILOT_POSE_H_

#include <mmpilot/math.h>


namespace mmpilot {

class PoseEN {
public:
	Vec2f pos = Vec2f::Zero();	// E, N [m]
	float yaw = 0;				// [rad]
	float alt = 0;				// over ground [m]
};

class PoseGPS {
public:
	double lat = 0;				// [rad]
	double lon = 0;				// [rad]
	double yaw = 0;				// [rad]
	double alt = 0;				// over ground [m]
};


} // mmpilot

#endif /* INCLUDE_MMPILOT_POSE_H_ */
