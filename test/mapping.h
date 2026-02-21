/*
 * mapping.h
 *
 *  Created on: Feb 21, 2026
 *      Author: mad
 */

#ifndef TEST_MAPPING_H_
#define TEST_MAPPING_H_

#include <mmpilot/mapping.h>

#include "pipeline.h"


class MappingPipe : public Pipeline {
public:
	float rebase_delta = 50;		// pixels traveled
	float rebase_scale = 1.25;		// relative scale change

	Mapping mapping;

protected:
	void init(int width, int height) override
	{
		Pipeline::init(width, height);

		mapping.init((src_width * 5) / 4, (src_height * 5) / 4, GL_RG);
	}

	void update() override
	{
		const auto H = get_params();
		const auto T = H.transform();

		std::cout << "homography: R_norm = " << H.R_norm << ", overlap = " << H.overlap << std::endl;

		mapping.render(source, H);

		if(T.pos.norm() > rebase_delta || T.scale > rebase_scale || T.scale < 1 / rebase_scale)
		{
			rebase();

			mapping.update(T);
		}

//		show(display, flip_image.out, {1, 0.2, 1, 1});
//		show(display, virtual_cam.out, {1, 0.1, 1, 1});
//		show(display, pyramid_filter.out[4], {1, 0.5, 1, 1});
//		show(display, stage[3]->smooth[1].out, {1, 0.1, 1, 1});
//		show(display, stage[0]->base_img, {0, 1, 1, 1});
//		show(display, stage[0]->solver.tex_debug, {1, 1, 1, 1});
		show(display, mapping.tex_debug, {1, 0, 1, 1});
//		show(display, mapping.finalize(), {1, 0, 0, 1});
	}


};



#endif /* TEST_MAPPING_H_ */
