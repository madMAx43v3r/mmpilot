/*
 * multi_affine.h
 *
 *  Created on: Feb 28, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_MULTI_AFFINE_H_
#define INCLUDE_MMPILOT_MULTI_AFFINE_H_

#include <mmpilot/texture.h>
#include <mmpilot/opengl.h>
#include <mmpilot/render.h>
#include <mmpilot/util.h>
#include <mmpilot/pyramid.h>
#include <mmpilot/gradient.h>
#include <mmpilot/affine.h>
#include <mmpilot/smooth.h>

#include <memory>
#include <vector>
#include <iostream>


namespace mmpilot {

class MultiAffine {
public:
	int depth = 4;

	float damping_xy = 1e-5;
	float damping_yaw = 1e-2;
	float damping_scale = 1e-2;

	bool debug = false;

	std::vector<int> num_iters = {1, 3, 7, 12};

	PyramidFilter pyramid[2];

	class Level {
	public:
		int num_smooth = -1;

		bool debug = false;

		SmoothFilter smooth[3];
		GradientFilter gradient;
		Affine solver;

		Affine::Params A;

		std::shared_ptr<Level> prev;	// lower scale

		void init(int level, int width, int height)
		{
			if(num_smooth < 0) {
				switch(level) {
					case 0:  num_smooth = 1; break;
					case 1:  num_smooth = 2; break;
					default: num_smooth = 3;
				}
			}
			solver.debug = debug;
			solver.init(width, height);

			for(int i = 0; i < num_smooth; ++i) {
				smooth[i].init(width, height, GL_RG16F, GL_RG, GL_HALF_FLOAT);
			}
			gradient.init(width, height);
		}

		void exec_ref(std::shared_ptr<GL_Tex2D> ref_)
		{
			auto ref = ref_;
			for(int i = 0; i < num_smooth; ++i) {
				smooth[i].exec(ref, false);
				ref = smooth[i].out;
			}
			gradient.exec(ref, false);
		}

		void exec_img(std::shared_ptr<GL_Tex2D> img, const bool sync)
		{
			if(prev) {
				A = copy(prev->A).scale(2);
				// cancel if previous level didn't converge
				if(!A.converged) {
					return;
				}
			} else {
				std::cout << "init_p: " << to_string(A) << std::endl;
			}
			A = solver.exec(gradient.out, img, A, sync);

			if(prev) {
				// we only care about top level
				A.converged = true;
			}
		}

		void exec(std::shared_ptr<GL_Tex2D> ref, std::shared_ptr<GL_Tex2D> img, const bool sync)
		{
			exec_ref(ref);
			exec_img(img, sync);
		}
	};

	std::vector<std::shared_ptr<Level>> stage;

	void init(int width_, int height_, int width_ref = width_, int height_ref = height_)
	{
		if(have_init) {
			throw std::logic_error("already initialized");
		}
		if(depth < 1) {
			throw std::logic_error("depth < 1");
		}
		width = width_;
		height = height_;

		pyramid[0].depth = depth;
		pyramid[1].depth = depth;
		pyramid[0].init(width, height, GL_RG16F, GL_RG, GL_HALF_FLOAT);
		pyramid[1].init(width_ref, height_ref, GL_RG16F, GL_RG, GL_HALF_FLOAT);

		int w = width;
		int h = height;
		for(int i = 0; i < depth; ++i)
		{
			auto lvl = std::make_shared<Level>();
			lvl->debug = debug;
			lvl->solver.damping_xy = damping_xy;
			lvl->solver.damping_yaw = damping_yaw;
			lvl->solver.damping_scale = damping_scale;
			lvl->solver.num_iters = num_iters[std::min(size_t(i), num_iters.size() - 1)];
			lvl->init(i, w, h);
			stage.push_back(lvl);

			w /= 2;
			h /= 2;
		}

		for(int i = 1; i < depth; ++i) {
			stage[i-1]->prev = stage[i];
		}

		have_init = true;
	}

	void exec_ref(std::shared_ptr<GL_Tex2D> ref)
	{
		if(!have_init) {
			throw std::logic_error("not initialized");
		}
		pyramid[0].exec(ref, false);

		for(int i = 0; i < depth; ++i) {
			stage[i]->exec_ref(pyramid[0].out[i]);
		}
	}

	Affine::Params exec_img(
			std::shared_ptr<GL_Tex2D> img,
			const Affine::Params& A = {}, const int level = 0, const bool sync = true)
	{
		if(!have_init) {
			throw std::logic_error("not initialized");
		}
		const auto begin = get_time_micros();

		pyramid[1].exec(img, false);

		// initial guess
		stage[0]->A = A;

		// back propagate initial guess
		for(int i = 1; i < depth; ++i) {
			stage[i]->A = copy(stage[i-1]->A).scale(0.5);
		}

		// top down processing
		for(int i = depth - 1; i >= level; --i)
		{
			stage[i]->exec_img(pyramid[1].out[i], sync);
		}

		// extrapolate to top level
		for(int i = std::min(level - 1, depth - 2); i >= 0; --i)
		{
			stage[i]->A = copy(stage[i+1]->A).scale(2);
		}

		if(sync) {
			GL_finish("MultiAffine::exec()");

			std::cout << "MultiAffine[" << width << "x" << height << "]: took "
					<< (get_time_micros() - begin) / 1000.f << " ms" << std::endl;
		}
		return stage[0]->A;
	}

	Affine::Params exec(
			std::shared_ptr<GL_Tex2D> ref, std::shared_ptr<GL_Tex2D> img,
			const Affine::Params& A = {}, const int level = 0, const bool sync = true)
	{
		exec_ref(ref);
		return exec_img(img, A, level, sync);
	}

private:
	int width = 0;
	int height = 0;

	GLuint fbo = 0;
	GLuint prog = 0;

	bool have_init = false;

};


} // mmpilot

#endif /* INCLUDE_MMPILOT_MULTI_AFFINE_H_ */
