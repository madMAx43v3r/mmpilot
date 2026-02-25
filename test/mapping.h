/*
 * mapping.h
 *
 *  Created on: Feb 21, 2026
 *      Author: mad
 */

#ifndef TEST_MAPPING_H_
#define TEST_MAPPING_H_

#include <mmpilot/mapping.h>

#include "pipeline2.h"


class MappingPipe : public Pipeline {
public:
	Mapping mapping;

protected:
	void init(int width, int height) override
	{
		Pipeline::init(width, height);

		mapping.init(src_width, src_height, GL_RG);
	}

	void update() override
	{
		const auto H = get_params();

		std::cout << "homography: R_norm = " << H.R_norm << ", overlap = " << H.overlap << std::endl;

		mapping.update(source, H);

		rebase();

//		show(display, mapping.tex_debug, {1, 0, 1, 1});
		show(display, mapping.finalize());
	}


};



#endif /* TEST_MAPPING_H_ */
