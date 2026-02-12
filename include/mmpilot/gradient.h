/*
 * gradient.h
 *
 *  Created on: Feb 12, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_GRADIENT_H_
#define INCLUDE_MMPILOT_GRADIENT_H_

#include <mmpilot/texture.h>
#include <mmpilot/opengl.h>
#include <mmpilot/render.h>
#include <mmpilot/util.h>

#include <memory>
#include <vector>
#include <iostream>


namespace mmpilot {

class GradientFilter {
public:
	int win_size = 7;		// window size

	std::shared_ptr<GL_Tex2D> tmp;		// (Y, Sx, Dx, w)
	std::shared_ptr<GL_Tex2D> out;		// (Y, Ix, Iy, w)

	void init(int width_, int height_)
	{
		if(have_init) {
			throw std::logic_error("already initialized");
		}
		width = width_;
		height = height_;

		const auto size_str = std::to_string(win_size);
		const auto H_file = "shader/gradient/H" + size_str + size_str + ".glsl";
		const auto G_file = "shader/gradient/G" + size_str + size_str + ".glsl";

		const auto vs = render::get_fullscreen_vertex_shader();
		const auto fs = GL_compile_shader(GL_FRAGMENT_SHADER, G_file);
		const auto fs_tmp = GL_compile_shader(GL_FRAGMENT_SHADER, H_file);
		prog = GL_link_program(vs, fs);
		prog_tmp = GL_link_program(vs, fs_tmp);

		tmp = std::make_shared<GL_Tex2D>(width, height, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
		out = std::make_shared<GL_Tex2D>(width, height, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);

		fbo = GL_create_FBO(out->id);
		fbo_tmp = GL_create_FBO(tmp->id);

		have_init = true;
	}

	void exec(std::shared_ptr<GL_Tex2D> in)
	{
		if(!have_init) {
			init(in->width, in->height);
		}
		const auto begin = get_time_micros();

		glUseProgram(prog_tmp);

		GL_bind_tex(prog_tmp, "uSrc", in->id, 0);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

		GL_uniform_2f(prog_tmp, "uInvSize", 1.f / width, 1.f / height);

		render::fullscreen(fbo_tmp, width, height);

		glUseProgram(prog);

		GL_bind_tex(prog, "uTmp", tmp->id, 0);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

		GL_uniform_2f(prog, "uInvSize", 1.f / width, 1.f / height);

		render::fullscreen(fbo, width, height);

		GL_finish("GradientFilter::handle()");

		std::cerr << "GradientFilter[" << width << "x" << height << "]: took "
				<< (get_time_micros() - begin) / 1000.f << " ms" << std::endl;
	}

private:
	int width = 0;
	int height = 0;

	GLuint fbo = 0;
	GLuint fbo_tmp = 0;
	GLuint prog = 0;
	GLuint prog_tmp = 0;

	bool have_init = false;

};


} // mmpilot

#endif /* INCLUDE_MMPILOT_GRADIENT_H_ */
