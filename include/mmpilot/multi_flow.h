/*
 * multi_flow.h
 *
 *  Created on: Feb 23, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_MULTI_FLOW_H_
#define INCLUDE_MMPILOT_MULTI_FLOW_H_

#include <mmpilot/texture.h>
#include <mmpilot/opengl.h>
#include <mmpilot/render.h>
#include <mmpilot/util.h>
#include <mmpilot/pyramid.h>
#include <mmpilot/rescale.h>
#include <mmpilot/gradient.h>
#include <mmpilot/flow.h>

#include <memory>
#include <vector>
#include <iostream>


namespace mmpilot {

class MultiFlowFilter {
public:
	int depth = 4;

	bool debug = false;

	PyramidFilter pyramid[2];

	class Level {
	public:
		bool debug = false;

		RescaleFilter upscale[2];
		SmoothFilter smooth[2];
		GradientFilter gradient[2];
		FlowFilter flow[2];

		std::shared_ptr<Level> prev;	// lower scale

		void init(int width, int height)
		{
			for(int i = 0; i < 2; ++i) {
				flow[i].debug = debug;
				flow[i].init(width, height);
				smooth[i].init(width, height, GL_RG16F, GL_RG, GL_HALF_FLOAT);
				gradient[i].init(width, height);
				upscale[i].init(width, height, GL_RG16F, GL_RG, GL_HALF_FLOAT);
			}
		}

		void exec(std::shared_ptr<GL_Tex2D> ref, std::shared_ptr<GL_Tex2D> img)
		{
			if(prev) {
				upscale[0].exec(prev->flow[0].out, false);
				upscale[1].exec(prev->flow[1].out, false);
			}
			smooth[0].exec(ref, false);
			smooth[1].exec(img, false);

			gradient[0].exec(smooth[0].out, false);
			gradient[1].exec(smooth[1].out, false);

			flow[0].exec(gradient[1].out, smooth[0].out, prev ? upscale[0].out : nullptr);
			flow[1].exec(gradient[0].out, smooth[1].out, prev ? upscale[1].out : nullptr);
		}
	};

	std::vector<std::shared_ptr<Level>> stage;

	std::shared_ptr<GL_Tex2D> out[2];		// (reverse, forward)

	void init(int width_, int height_)
	{
		if(have_init) {
			throw std::logic_error("already initialized");
		}
		if(depth < 1) {
			throw std::logic_error("depth < 1");
		}
		width = width_;
		height = height_;

		for(int i = 0; i < 2; ++i) {
			pyramid[i].depth = depth;
			pyramid[i].init(width, height, GL_RG16F, GL_RG, GL_HALF_FLOAT);
		}

		int w = width;
		int h = height;
		for(int i = 0; i < depth; ++i)
		{
			auto lvl = std::make_shared<Level>();
			lvl->debug = debug;
			lvl->init(w, h);
			stage.push_back(lvl);

			w /= 2;
			h /= 2;
		}

		for(int i = 1; i < depth; ++i) {
			stage[i-1]->prev = stage[i];
		}

		have_init = true;
	}

	void exec(std::shared_ptr<GL_Tex2D> ref, std::shared_ptr<GL_Tex2D> img, const bool sync = true)
	{
		if(!have_init) {
			throw std::logic_error("not initialized");
		}
		const auto begin = get_time_micros();

		pyramid[0].exec(ref);
		pyramid[1].exec(img);

		// top down processing
		for(int i = depth - 1; i >= 0; --i)
		{
			stage[i]->exec(pyramid[0].out[i], pyramid[1].out[i]);
		}

		out[0] = stage[0]->flow[0].out;
		out[1] = stage[0]->flow[1].out;

		if(sync) {
			GL_finish("MultiFlowFilter::exec()");

			std::cout << "MultiFlowFilter[" << width << "x" << height << "]: took "
					<< (get_time_micros() - begin) / 1000.f << " ms" << std::endl;
		}
	}

private:
	int width = 0;
	int height = 0;

	GLuint fbo = 0;
	GLuint prog = 0;

	bool have_init = false;

};


} // mmpilot

#endif /* INCLUDE_MMPILOT_MULTI_FLOW_H_ */
