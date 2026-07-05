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
#include <mmpilot/agl_stage.h>
#include <mmpilot/model_stage.h>
#include <mmpilot/control_stage.h>
#include <mmpilot/beta_msp.h>


namespace mmpilot {

class NavigationBase {
public:
	Pipeline pipe;

	VirtualCamStage virtual_cam;
	AffineStage affine;
	AGL_Stage agl;
	ModelStage model;

	NavigationBase() {
		pipe.add_stage(&virtual_cam);
		pipe.add_stage(&affine);
		pipe.add_stage(&agl);
		pipe.add_stage(&model);
	}

	void init(int width, int height) {
		pipe.init_sync(width, height);
	}

};


class Navigation : public NavigationBase {
public:
	ControlStage control;

	Navigation(MSP2* msp)
		:	control(msp)
	{
		pipe.add_stage(&control);
	}

};



} // mmpilot

#endif /* INCLUDE_MMPILOT_NAVIGATION_H_ */
