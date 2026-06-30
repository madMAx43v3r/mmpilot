/*
 * nav_point.h
 *
 *  Created on: Jun 30, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_NAV_POINT_H_
#define INCLUDE_MMPILOT_NAV_POINT_H_

#include <mmpilot/stage.h>


namespace mmpilot {

class NavPoint : public Stage {
public:
	double gain = 1;
	double damping = 1;

	NavPoint() : Stage("nav_point") {}

	ConstPointer out_cmd;		// PositionControl | VelocityControl


	void init() override
	{
		add_output("control", out_cmd);
	}

	void exec() override
	{

	}



};



} // mmpilot

#endif /* INCLUDE_MMPILOT_NAV_POINT_H_ */
