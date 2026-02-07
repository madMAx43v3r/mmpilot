/*
 * camera.h
 *
 *  Created on: Feb 7, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_CAMERA_H_
#define INCLUDE_MMPILOT_CAMERA_H_


namespace mmpilot {

class Camera {
public:
	Camera();

	void run();

private:
	bool do_run = true;

};


} // mmpilot

#endif /* INCLUDE_MMPILOT_CAMERA_H_ */
