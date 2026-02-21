/*
 * calib.h
 *
 *  Created on: Feb 21, 2026
 *      Author: mad
 */

#ifndef TEST_CALIB_H_
#define TEST_CALIB_H_

#include "pipeline.h"


class CalibrationPipe : public Pipeline {
public:
	float trigger_delta = 50;		// pixels traveled
	float trigger_scale = 1.25;		// relative scale change

protected:
	void init(int width, int height) override
	{
		Pipeline::init(width, height);

		mapping.init((width * 5) / 4, (height * 5) / 4, GL_RG);
	}

	void update() override
	{
		const auto H = stage[0]->H;
		const auto T = H.transform();

		if(T.pos.norm() > trigger_delta || T.scale > trigger_scale || T.scale < 1 / trigger_scale)
		{
			rebase();


		}

//		show(display, flip_image.out, {1, 0.2, 1, 1});
//		show(display, virtual_cam.out, {1, 0.1, 1, 1});
//		show(display, pyramid_filter.out[4], {1, 0.5, 1, 1});
//		show(display, stage[3]->smooth[1].out, {1, 0.1, 1, 1});
//		show(display, stage[0]->base_img, {0, 1, 1, 1});
		show(display, stage[0]->solver.tex_debug, {1, 1, 1, 1});
//		show(display, mapping.tex_debug, {1, 0, 1, 1});
//		show(display, mapping.finalize(), {1, 0, 0, 1});
	}


};



#endif /* TEST_CALIB_H_ */
