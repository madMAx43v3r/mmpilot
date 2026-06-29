/*
 * smooth.h
 *
 *  Created on: Feb 12, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_SMOOTH_H_
#define INCLUDE_MMPILOT_SMOOTH_H_

#include <mmpilot/texture.h>
#include <mmpilot/opengl.h>
#include <mmpilot/render.h>
#include <mmpilot/util.h>

#include <memory>
#include <vector>
#include <iostream>


namespace mmpilot {

class SmoothFilter {
public:
	int win_size = 5;

	std::shared_ptr<GL_Tex2D> out;

	void init(int width_, int height_, GLenum int_format, GLenum format, GLenum type)
	{
		if(have_init) {
			throw std::logic_error("already initialized");
		}
		width = width_;
		height = height_;

		std::string shader = "smooth";
		switch(win_size) {
			case 5: shader += "55"; break;
			default:
				throw std::logic_error("SmoothFilter: invalid win_size");
		}
		switch(format) {
			case GL_RED:  shader += "_r"; break;
			case GL_RG:   shader += "_rg"; break;
			case GL_RGBA: shader += "_rgba"; break;
			default:
				throw std::runtime_error("SmoothFilter: invalid format");
		}

		const auto vs = render::fullscreen_vertex_shader();
		const auto fs = GL_compile_shader(GL_FRAGMENT_SHADER, "shader/smooth/" + shader + ".glsl");
		prog = GL_link_program(vs, fs);

		out = std::make_shared<GL_Tex2D>(width, height, int_format, format, type);
		fbo = GL_create_FBO(out->id);

		have_init = true;
	}

	void exec(std::shared_ptr<const GL_Tex2D> in, const bool sync = true)
	{
		if(!have_init) {
			init(in->width, in->height, in->internal_fmt, in->format, in->type);
		}
		const auto begin = get_time_micros();

		glUseProgram(prog);

		GL_bind_tex(prog, "uSrc", in->id, 0);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

		GL_uniform_2f(prog, "uInvSize", 1.f / width, 1.f / height);

		render::fullscreen(fbo, width, height);

		if(sync) {
			GL_finish("SmoothFilter::exec()");

			std::cout << "SmoothFilter[" << width << "x" << height << "]: took "
					<< (get_time_micros() - begin) / 1000.f << " ms" << std::endl;
		}
	}

	void clear() {
		render::clear(fbo, width, height);
	}

private:
	int width = 0;
	int height = 0;

	GLuint fbo = 0;
	GLuint prog = 0;

	bool have_init = false;

};


} // mmpilot



#endif /* INCLUDE_MMPILOT_SMOOTH_H_ */
