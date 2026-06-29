/*
 * affine_stage.h
 *
 *  Created on: Jun 29, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_AFFINE_STAGE_H_
#define INCLUDE_MMPILOT_AFFINE_STAGE_H_

#include <mmpilot/stage.h>
#include <mmpilot/smooth.h>
#include <mmpilot/gradient.h>
#include <mmpilot/affine.h>
#include <mmpilot/pyramid.h>


namespace mmpilot {

class AffineStage : public Stage {
public:
	int pyramid_depth = 4;

	std::vector<int> num_iters = {1, 2, 5, 20};

	bool is_debug = false;


	AffineStage() : Stage("affine") {}

	void reset(const Affine::Params& H)
	{
		stage[0]->A = H;

		for(int i = 1; i < pyramid_depth; ++i) {
			stage[i]->A = copy(stage[i-1]->A).scale(0.5);
		}
	}

	Affine::Params get_params(const int i = 0) const
	{
		if(i < 0 || i >= pyramid_depth) {
			throw std::logic_error("get_params(): out of bounds");
		}
		return stage[i]->A;
	}

	class Level {
	public:
		int level = 0;
		int num_smooth = -1;

		SmoothFilter smooth[4];
		GradientFilter gradient;
		Affine solver;

		Affine::Params A;

		std::shared_ptr<Level> upper;			// lower scale (upper level)
		std::shared_ptr<GL_Tex2D> base_img;

		void init(int level, int width, int height)
		{
			this->level = level;

			if(num_smooth < 0) {
				switch(level) {
					case 0:  num_smooth = 1; break;
					case 1:  num_smooth = 2; break;
					case 2:  num_smooth = 3; break;
					default: num_smooth = 4;
				}
			}

			for(int i = 0; i < num_smooth; ++i) {
				smooth[i].init(width, height, GL_RG16F, GL_RG, GL_HALF_FLOAT);
			}

			gradient.init(width, height);
			solver.init(width, height);

			base_img = std::make_shared<GL_Tex2D>(width, height, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);

			fbo_copy[0] = GL_create_FBO(base_img->id);
			fbo_copy[1] = GL_create_FBO(gradient.out->id);
		}

		void exec(std::shared_ptr<const GL_Tex2D> img)
		{
			if(have_base) {
				if(upper) {
					A = copy(upper->A).scale(2);
				}
				A = solver.exec(base_img, img, A);

				std::cout << "params[" << level << "][" << solver.num_iters << "] = " << to_string(A) << std::endl;
			} else {
				rebase(img);
			}
		}

		void rebase(std::shared_ptr<const GL_Tex2D> img)
		{
			auto in = img;
			for(int i = 0; i < num_smooth; ++i) {
				smooth[i].exec(in);
				in = smooth[i].out;
			}
			gradient.exec(in);

			glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo_copy[1]);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo_copy[0]);
			glBlitFramebuffer(
				0, 0, base_img->width, base_img->height,
				0, 0, base_img->width, base_img->height,
				GL_COLOR_BUFFER_BIT, GL_NEAREST
			);

			A = Affine::Params();
			have_base = true;
		}

	private:
		bool have_base = false;

		GLuint fbo_copy[2] = {};
	};


	PyramidFilter pyramid;

	std::shared_ptr<const GL_Tex2D> input;

	std::vector<std::shared_ptr<Level>> stage;

	Affine::Params output;

	bool have_base = false;

private:
	void init() override
	{
		const int width  = get_input<Integer>("width");
		const int height = get_input<Integer>("height");

		input = get_input<ConstPointer>("image").get<GL_Tex2D>();

		pyramid.depth = pyramid_depth;
		pyramid.init(width, height, GL_RG16F, GL_RG, GL_HALF_FLOAT);

		int w = width;
		int h = height;
		for(int i = 0; i < pyramid_depth; ++i)
		{
			auto lvl = std::make_shared<Level>();
			lvl->solver.debug = is_debug;
			lvl->solver.num_iters = num_iters[std::min(size_t(i), num_iters.size() - 1)];

			lvl->init(i, w, h);
			stage.push_back(lvl);

			w /= 2;
			h /= 2;
		}

		for(int i = 0; i + 1 < pyramid_depth; ++i) {
			stage[i]->upper = stage[i + 1];
		}

		add_output("affine", &output);
	}

	void exec() override
	{
		pyramid.exec(input);

		if(!have_base) {
			rebase();
			have_base = true;
			return;
		}

		// top down processing
		for(int i = pyramid_depth - 1; i >= 0; --i) {
			stage[i]->exec(pyramid.out[i]);
		}

		// back propagate most accurate result
		for(int i = 1; i < pyramid_depth; ++i) {
			stage[i]->A = copy(stage[i-1]->A).scale(0.5);
		}

		output = get_params();

//		show(display, stage[0]->base_img);
//		show(display, stage[0]->solver.tex_debug);

		rebase();
	}

	void rebase()
	{
		for(int i = 0; i < pyramid_depth; ++i) {
			stage[i]->rebase(pyramid.out[i]);
		}
	}

};






} // mmpilot

#endif /* INCLUDE_MMPILOT_AFFINE_STAGE_H_ */
