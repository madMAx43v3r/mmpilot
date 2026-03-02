/*
 * merge.h
 *
 *  Created on: Feb 23, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_MERGE_H_
#define INCLUDE_MMPILOT_MERGE_H_

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

class MergeFilter {
public:
	int depth = 4;
	int num_iter = 3;
	int reduction_chunk = 16;

	float weight = 0.5;			// 0..1

	bool debug = false;

	MultiFlowFilter flow;

	std::shared_ptr<GL_Tex2D> out_blend;
	std::shared_ptr<GL_Tex2D> out_warp[2];

	std::shared_ptr<GL_Tex2D> tex_ref;
	std::shared_ptr<GL_Tex2D> tex_error;
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

		out_blend = std::make_shared<GL_Tex2D>(width, height, int_format, format, GL_HALF_FLOAT);
		tex_ref   = std::make_shared<GL_Tex2D>(width, height, int_format, format, GL_HALF_FLOAT);
		tex_error = std::make_shared<GL_Tex2D>(width, reduction_chunk, GL_RG32F, GL_RG, GL_FLOAT);

		fbo_out = GL_create_FBO(out_blend);
		fbo_ref = GL_create_FBO(tex_ref);
		fbo_error = GL_create_FBO(tex_error);

		const auto vs = render::fullscreen_vertex_shader();
		const auto fs_warp = GL_compile_shader(GL_FRAGMENT_SHADER, "shader/mapping/flow_warp.glsl");
		const auto fs_global_warp = GL_compile_shader(GL_FRAGMENT_SHADER, "shader/mapping/global_warp.glsl");
		const auto fs_blend = GL_compile_shader(GL_FRAGMENT_SHADER, "shader/mapping/blend_mono.glsl");
		const auto fs_error = GL_compile_shader(GL_FRAGMENT_SHADER, "shader/mapping/error_mono.glsl");
		const auto fs_debug = GL_compile_shader(GL_FRAGMENT_SHADER, "shader/debug/delta_mono.glsl");
		prog_warp = GL_link_program(vs, fs_warp);
		prog_blend = GL_link_program(vs, fs_blend);
		prog_error = GL_link_program(vs, fs_error);
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

	double exec(std::shared_ptr<GL_Tex2D> ref, std::shared_ptr<GL_Tex2D> img, const Affine::Params& A, const bool sync = true)
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

			glUseProgram(prog_warp);

			GL_uniform_2f(prog_warp, "uInvSize", 1. / width, 1. / height);

			// ----- warp reference
			GL_bind_tex(prog_warp, "uImg", in_ref, 0);

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

			GL_bind_tex(prog_warp, "uFlow", flow.out[1], 1);

			GL_uniform_1f(prog_warp, "uWeight", weight);

			render::fullscreen(fbo[k][0], width, height);

			// ----- warp image
			GL_bind_tex(prog_warp, "uImg", in_img, 0);

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

			GL_bind_tex(prog_warp, "uFlow", flow.out[0], 1);

			GL_uniform_1f(prog_warp, "uWeight", 1 - weight);

			render::fullscreen(fbo[k][1], width, height);

			in_ref = tex_buf[k][0];
			in_img = tex_buf[k][1];
		}

		out_warp[0] = in_ref;
		out_warp[1] = in_img;

		glUseProgram(prog_blend);

		GL_bind_tex(prog_blend, "uSrc0", in_ref, 0);
		GL_bind_tex(prog_blend, "uSrc1", in_img, 1);

		GL_uniform_1f(prog_blend, "uWeight", weight);

		render::fullscreen(fbo_out, width, height);

		glUseProgram(prog_error);

		GL_bind_tex(prog_error, "uSrc0", in_ref, 0);
		GL_bind_tex(prog_error, "uSrc1", in_img, 1);

		GL_uniform_1i(prog_error, "uHeight", height);
		GL_uniform_1i(prog_error, "uChunkSize", reduction_chunk);

		render::fullscreen(fbo_error, width, reduction_chunk);

		GL_finish("MergeFilter::exec()");

		GL_read_FBO_RG(fbo_error, 0, width, reduction_chunk, error_buf);

		// reduce over height first
		for(int y = 1; y < reduction_chunk; ++y) {
			for(int x = 0; x < width * 2; ++x) {
				error_buf[x] += error_buf[y * width * 2 + x];
			}
		}

		// reduce over width to get final error sum
		double error = 0;
		double total = 0;
		for(int x = 0; x < width; ++x) {
			error += error_buf[x * 2 + 0];
			total += error_buf[x * 2 + 1];
		}
		error = (1000 * error) / total;

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
			GL_finish("MergeFilter::exec()");

			std::cout << "MergeFilter[" << width << "x" << height << "]: took "
					<< (get_time_micros() - begin) / 1000.f << " ms" << std::endl;
		}
		return error;
	}

private:
	int width = 0;
	int height = 0;

	std::vector<float> error_buf;

	GLuint fbo[2][2] = {};
	GLuint fbo_ref = 0;
	GLuint fbo_out = 0;
	GLuint fbo_error = 0;
	GLuint fbo_debug[2] = {};

	GLuint prog_warp = 0;
	GLuint prog_blend = 0;
	GLuint prog_render = 0;
	GLuint prog_error = 0;
	GLuint prog_debug = 0;

	bool have_init = false;

};


} // mmpilot

#endif /* INCLUDE_MMPILOT_MERGE_H_ */
