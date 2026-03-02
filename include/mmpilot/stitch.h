/*
 * stitch.h
 *
 *  Created on: March 2, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_STITCH_H_
#define INCLUDE_MMPILOT_STITCH_H_

#include <mmpilot/texture.h>
#include <mmpilot/opengl.h>
#include <mmpilot/render.h>
#include <mmpilot/util.h>
#include <mmpilot/affine.h>
#include <mmpilot/multi_flow.h>

#include <memory>
#include <vector>
#include <iostream>


namespace mmpilot {

class StitchFilter {
public:
	int depth = 4;
	int num_iter = 3;

	bool debug = false;

	MultiFlowFilter flow;

	std::shared_ptr<GL_Tex2D> out[2];

	std::shared_ptr<GL_Tex2D> tex_ref;
	std::shared_ptr<GL_Tex2D> tex_debug[2];
	std::shared_ptr<GL_Tex2D> tex_buf[2][2];

	void init(int width_, int height_, GLenum format)
	{
		if(have_init) {
			throw std::logic_error("already initialized");
		}
		width = width_;
		height = height_;

		GLenum int_format;
		switch(format) {
			case GL_RG: int_format = GL_RG16F; break;
//			case GL_RGBA: int_format = GL_RGBA16F; break;
			default:
				throw std::logic_error("invalid format");
		}

		flow.depth = depth;
		flow.debug = debug;
		flow.init(width, height);

		for(int i = 0; i < 2; ++i) {
			for(int k = 0; k < 2; ++k) {
				tex_buf[i][k] = std::make_shared<GL_Tex2D>(width, height, int_format, format, GL_HALF_FLOAT);
				fbo[i][k] = GL_create_FBO(tex_buf[i][k]);
			}
		}

		tex_ref = std::make_shared<GL_Tex2D>(width, height, int_format, format, GL_HALF_FLOAT);
		fbo_ref = GL_create_FBO(tex_ref);

		const auto vs = render::fullscreen_vertex_shader();
		const auto fs_global_warp = GL_compile_shader(GL_FRAGMENT_SHADER, "shader/mapping/global_warp.glsl");
		const auto fs_stitch = GL_compile_shader(GL_FRAGMENT_SHADER, "shader/mapping/stitch_mono.glsl");
		const auto fs_debug = GL_compile_shader(GL_FRAGMENT_SHADER, "shader/debug/delta_mono.glsl");
		prog_stitch = GL_link_program(vs, fs_stitch);
		prog_render = GL_link_program(vs, fs_global_warp);
		prog_debug = GL_link_program(vs, fs_debug);

		if(debug) {
			for(int i = 0; i < 2; ++i) {
				tex_debug[i] = std::make_shared<GL_Tex2D>(width, height, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);
				fbo_debug[i] = GL_create_FBO(tex_debug[i]);
			}
		}
		have_init = true;
	}

	void exec(std::shared_ptr<GL_Tex2D> ref, std::shared_ptr<GL_Tex2D> img, const Affine::Params& A, const bool sync = true)
	{
		if(!have_init) {
			init(img->width, img->height, img->format);
		}
		const auto begin = get_time_micros();

		// project reference to new frame first
		glUseProgram(prog_render);

		GL_bind_tex(prog_render, "uSrc", ref, 0);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		GL_uniform_2f(prog_render, "uCenterIn",  ref->width / 2., ref->height / 2.);
		GL_uniform_2f(prog_render, "uCenterOut", img->width / 2., img->height / 2.);
		GL_uniform_2f(prog_render, "uInvSizeIn", 1. / ref->width, 1. / ref->height);
		GL_uniform_fv(prog_render, "uParams", A);

		render::fullscreen(fbo_ref, width, height);

		auto in_ref = tex_ref;
		auto in_img = img;

		for(int iter = 0; iter < num_iter; ++iter)
		{
			const int i = iter % 2;
			const int k = (iter + 1) % 2;

			flow.exec(in_ref, in_img, false);

			glUseProgram(prog_stitch);

			GL_uniform_2f(prog_stitch, "uInvSize", 1. / width, 1. / height);

			// ----- warp reference
			GL_bind_tex(prog_stitch, "uImg", in_ref, 0);

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

			GL_bind_tex(prog_stitch, "uFlow", flow.out[1], 1);

			render::fullscreen(fbo[k][0], width, height);

			// ----- warp image
			GL_bind_tex(prog_stitch, "uImg", in_img, 0);

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

			GL_bind_tex(prog_stitch, "uFlow", flow.out[0], 1);

			render::fullscreen(fbo[k][1], width, height);

			in_ref = tex_buf[k][0];
			in_img = tex_buf[k][1];
		}

		out[0] = in_ref;
		out[1] = in_img;

		if(debug)
		{
			glUseProgram(prog_debug);

			GL_bind_tex(prog_debug, "uImg0", in_img, 0);
			GL_bind_tex(prog_debug, "uImg1", in_ref, 1);

			render::fullscreen(fbo_debug[0], width, height);

			GL_bind_tex(prog_debug, "uImg0", in_ref, 0);
			GL_bind_tex(prog_debug, "uImg1", in_img, 1);

			render::fullscreen(fbo_debug[1], width, height);
		}

		if(sync) {
			GL_finish("StitchFilter::exec()");

			std::cout << "StitchFilter[" << width << "x" << height << "]: took "
					<< (get_time_micros() - begin) / 1000.f << " ms" << std::endl;
		}
	}

private:
	int width = 0;
	int height = 0;

	GLuint fbo[2][2] = {};
	GLuint fbo_ref = 0;
	GLuint fbo_debug[2] = {};

	GLuint prog_stitch = 0;
	GLuint prog_render = 0;
	GLuint prog_debug = 0;

	bool have_init = false;

};


} // mmpilot

#endif /* INCLUDE_MMPILOT_STITCH_H_ */
