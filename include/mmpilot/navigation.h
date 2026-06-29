/*
 * navigation.h
 *
 *  Created on: Jun 29, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_NAVIGATION_H_
#define INCLUDE_MMPILOT_NAVIGATION_H_

#include <mmpilot/pipeline.h>
#include <mmpilot/virtual_cam_stage.h>
#include <mmpilot/affine_stage.h>


namespace mmpilot {

class NavigationBase {
public:
	Pipeline pipe;

	VirtualCamStage virtual_cam;
	AffineStage affine;

	NavigationBase() {
		pipe.add_stage(&virtual_cam);
		pipe.add_stage(&affine);
	}

	void init(int width, int height) {
		pipe.init_sync(width, height);
	}

	void exec() {
		pipe.exec();
	}

};


class Navigation : public NavigationBase {
public:
	// ControlStage control;

	Navigation() {
//		pipe.add_stage(&control);
	}

};



} // mmpilot

#endif /* INCLUDE_MMPILOT_NAVIGATION_H_ */
