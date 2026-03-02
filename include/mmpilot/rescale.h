/*
 * rescale.h
 *
 *  Created on: Feb 23, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_RESCALE_H_
#define INCLUDE_MMPILOT_RESCALE_H_

#include <mmpilot/texture.h>
#include <mmpilot/opengl.h>
#include <mmpilot/render.h>
#include <mmpilot/util.h>

#include <memory>
#include <vector>
#include <iostream>


namespace mmpilot {

class RescaleFilter {
public:
	std::shared_ptr<GL_Tex2D> out;

	void init(int width_, int height_, GLenum internal_fmt, GLenum format, GLenum type)
	{
		if(have_init) {
			throw std::logic_error("already initialized");
		}
		width = width_;
		height = height_;

		const auto vs = render::fullscreen_vertex_shader();
		const auto fs = GL_compile_shader(GL_FRAGMENT_SHADER, "shader/scale/rescale.glsl");
		prog = GL_link_program(vs, fs);

		out = std::make_shared<GL_Tex2D>(width, height, internal_fmt, format, type);
		fbo = GL_create_FBO(out->id);

		have_init = true;
	}

	void exec(std::shared_ptr<GL_Tex2D> in, const bool sync = true)
	{
		if(!have_init) {
			throw std::logic_error("not initialized");
		}
		const auto begin = get_time_micros();

		glUseProgram(prog);

		GL_bind_tex(prog, "uSrc", in, 0);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		GL_uniform_2f(prog, "uInvOutSize", 1. / width, 1. / height);

		render::fullscreen(fbo, width, height);

		if(sync) {
			GL_finish("RescaleFilter::exec()");

			std::cout << "RescaleFilter[" << width << "x" << height << "]: took "
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

#endif /* INCLUDE_MMPILOT_RESCALE_H_ */
