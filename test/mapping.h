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
		const auto A = get_params();

		std::cout << "affine: R_norm = " << A.R_norm << ", overlap = " << A.overlap << std::endl;

		mapping.update(ts, source, A);

		rebase();

		const auto map = mapping.finalize();

//		show(display, stage[0]->solver.tex_debug);
//		show(display, mapping.tex_debug, {1, 0, 1, 1});
		show(display, map);
	}

	void on_sample(std::shared_ptr<Sample> sample) override
	{
		Pipeline::on_sample(sample);

		if(auto gps = std::dynamic_pointer_cast<MSP2Client::RawGPS>(sample)) {
			mapping.on_gps(gps);
		}
	}


};



#endif /* TEST_MAPPING_H_ */
