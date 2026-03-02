/*
 * flip.h
 *
 *  Created on: Feb 14, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_FLIP_H_
#define INCLUDE_MMPILOT_FLIP_H_

#include <mmpilot/texture.h>
#include <mmpilot/opengl.h>
#include <mmpilot/render.h>
#include <mmpilot/util.h>

#include <memory>
#include <vector>
#include <iostream>


namespace mmpilot {

class FlipImage {
public:
	bool flip_x = false;
	bool flip_y = false;

	std::shared_ptr<GL_Tex2D> out;

	void init(int width_, int height_, GLenum int_format, GLenum format, GLenum type)
	{
		if(have_init) {
			throw std::logic_error("already initialized");
		}
		width = width_;
		height = height_;

		const auto vs = render::fullscreen_vertex_shader();
		const auto fs = GL_compile_shader(GL_FRAGMENT_SHADER, "shader/color/flip_xy.glsl");
		prog = GL_link_program(vs, fs);

		out = std::make_shared<GL_Tex2D>(width, height, int_format, format, type);
		fbo = GL_create_FBO(out->id);

		have_init = true;
	}

	void exec(std::shared_ptr<GL_Tex2D> in)
	{
		if(!have_init) {
			init(in->width, in->height, in->internal_fmt, in->format, in->type);
		}
		const auto begin = get_time_micros();

		glUseProgram(prog);

		GL_bind_tex(prog, "uSrc", in->id, 0);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

		GL_uniform_2i(prog, "uSize", width, height);
		GL_uniform_1i(prog, "uFlipX", flip_x ? 1 : 0);
		GL_uniform_1i(prog, "uFlipY", flip_y ? 1 : 0);

		render::fullscreen(fbo, width, height);

		GL_finish("FlipImage::exec()");

		std::cout << "FlipImage[" << width << "x" << height << "]: took "
				<< (get_time_micros() - begin) / 1000.f << " ms" << std::endl;
	}

private:
	int width = 0;
	int height = 0;

	GLuint fbo = 0;
	GLuint prog = 0;

	bool have_init = false;

};


} // mmpilot

#endif /* INCLUDE_MMPILOT_FLIP_H_ */
