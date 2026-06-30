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

		mapping.merge.debug = true;
		mapping.affine.debug = true;

		mapping.init(src_width, src_height, GL_RG);
	}

	void update() override
	{
		const auto A = get_params();

		mapping.exec(ts, source, A);

		rebase();

		const bool done = mapping.nodes.size() >= 150;

		mapping.finalize(done ? 2 : 0);

//		show(display, stage[0]->solver.tex_debug);
//		show(display, mapping.merge.tex_debug[0]);
//		show(display, mapping.merge.flow.stage[0]->flow[1].tex_debug);
//		show(display, mapping.affine.stage[0]->solver.tex_debug);
		show(display, mapping.render_map());

		if(done) {
			if(auto map = mapping.map) {
				Recorder rec("map.dat");
				map->write(rec);
			}
			bool dummy;
			std::cin >> dummy;
			::exit(0);
		}
	}

	void on_sample(std::shared_ptr<Sample> sample) override
	{
		Pipeline::on_sample(sample);

		if(auto gps = std::dynamic_pointer_cast<MSP2::RawGPS>(sample)) {
			mapping.on_gps(gps);
		}
	}


};



#endif /* TEST_MAPPING_H_ */
