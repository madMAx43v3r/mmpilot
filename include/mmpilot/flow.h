/*
 * flow.h
 *
 *  Created on: Feb 23, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_FLOW_H_
#define INCLUDE_MMPILOT_FLOW_H_

#include <mmpilot/texture.h>
#include <mmpilot/opengl.h>
#include <mmpilot/render.h>
#include <mmpilot/util.h>
#include <mmpilot/smooth.h>
#include <mmpilot/homography.h>

#include <memory>
#include <vector>
#include <iostream>


namespace mmpilot {

class FlowFilter {
public:
	int num_iter = 3;

	float damping = 1e-4;
	float min_det = 1e-8;

	bool debug = false;

	SmoothFilter smooth;

	std::shared_ptr<GL_Tex2D> out;
	std::shared_ptr<GL_Tex2D> tex_buf;
	std::shared_ptr<GL_Tex2D> tex_debug;

	void init(int width_, int height_)
	{
		if(have_init) {
			throw std::logic_error("already initialized");
		}
		width = width_;
		height = height_;

		smooth.init(width, height, GL_RG16F, GL_RG, GL_HALF_FLOAT);

		const auto vs = render::get_fullscreen_vertex_shader();
		const auto fs = GL_compile_shader(GL_FRAGMENT_SHADER, "shader/mapping/flow_mono.glsl");
		const auto fs_init = GL_compile_shader(GL_FRAGMENT_SHADER, "shader/mapping/flow_init.glsl");
		const auto fs_debug = GL_compile_shader(GL_FRAGMENT_SHADER, "shader/debug/flow_overlay.glsl");
		prog = GL_link_program(vs, fs);
		prog_init = GL_link_program(vs, fs_init);
		prog_debug = GL_link_program(vs, fs_debug);

		tex_buf = std::make_shared<GL_Tex2D>(width, height, GL_RG16F, GL_RG, GL_HALF_FLOAT);
		fbo = GL_create_FBO(tex_buf);

		if(debug) {
			tex_debug = std::make_shared<GL_Tex2D>(width, height, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);
			fbo_debug = GL_create_FBO(tex_debug);
		}
		have_init = true;
	}

	void exec(std::shared_ptr<GL_Tex2D> ref, std::shared_ptr<GL_Tex2D> img, const Homography::Params& H = {})
	{
		if(!have_init) {
			init(img->width, img->height);
		}
		const auto begin = get_time_micros();

		glUseProgram(prog_init);

		GL_uniform_2f(prog_init, "uCenter", width / 2., height / 2.);
		GL_uniform_fv(prog_init, "uParams", H);

		render::fullscreen(fbo, width, height);

		out = tex_buf;

		glUseProgram(prog);

		GL_bind_tex(prog, "uRef", ref, 0);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		GL_bind_tex(prog, "uImg", img, 1);

		GL_uniform_2f(prog, "uInvSize", 1. / width, 1. / height);
		GL_uniform_1f(prog, "uDamping", damping);
		GL_uniform_1f(prog, "uMinDet", min_det);

		for(int iter = 0; iter < num_iter; ++iter)
		{
			GL_bind_tex(prog, "uFlow", out, 2);

			render::fullscreen(fbo, width, height);

			smooth.exec(tex_buf, false);
			out = smooth.out;
		}

		if(debug)
		{
			glUseProgram(prog_debug);

			GL_bind_tex(prog_debug, "uImg", img, 0);
			GL_bind_tex(prog_debug, "uFlow", out, 1);

			GL_uniform_2f(prog_debug, "uCenter", width / 2., height / 2.);
			GL_uniform_fv(prog_debug, "uParams", H);

			render::fullscreen(fbo_debug, width, height);
		}

		GL_finish("FlowFilter::exec()");

		std::cout << "FlowFilter[" << width << "x" << height << "]: took "
				<< (get_time_micros() - begin) / 1000.f << " ms" << std::endl;
	}

private:
	int width = 0;
	int height = 0;

	GLuint fbo = 0;
	GLuint fbo_debug = 0;
	GLuint prog = 0;
	GLuint prog_init = 0;
	GLuint prog_debug = 0;

	bool have_init = false;

};


} // mmpilot

#endif /* INCLUDE_MMPILOT_FLOW_H_ */
