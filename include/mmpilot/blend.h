/*
 * blend.h
 *
 *  Created on: Jun 28, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_BLEND_H_
#define INCLUDE_MMPILOT_BLEND_H_


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

class BlendFilter {
public:
	float weight = 0.5;			// 0..1

	std::shared_ptr<GL_Tex2D> out;

	void init(int width_, int height_, GLenum format)
	{
		if(have_init) {
			throw std::logic_error("already initialized");
		}
		width = width_;
		height = height_;

		GLenum int_format;
		switch(format) {
			case GL_RED: int_format = GL_R16F; break;
			case GL_RG: int_format = GL_RG16F; break;
			case GL_RGBA: int_format = GL_RGBA16F; break;
			default:
				throw std::logic_error("invalid format");
		}

		out = std::make_shared<GL_Tex2D>(width, height, int_format, format, GL_HALF_FLOAT);

		fbo = GL_create_FBO(out);

		const auto vs = render::fullscreen_vertex_shader();
		const auto fs = GL_compile_shader(GL_FRAGMENT_SHADER, "shader/color/blend.glsl");
		prog = GL_link_program(vs, fs);

		have_init = true;
	}

	void exec(std::shared_ptr<GL_Tex2D> L, std::shared_ptr<GL_Tex2D> R, const bool sync = true)
	{
		if(!have_init) {
			init(L->width, L->height, L->format);
		}
		const auto begin = get_time_micros();

		glUseProgram(prog);

		GL_bind_tex(prog, "uSrc0", L, 0);
		GL_bind_tex(prog, "uSrc1", R, 1);

		GL_uniform_1f(prog, "uFactor", weight);

		render::fullscreen(fbo, width, height);

		if(sync) {
			GL_finish("BlendFilter::exec()");

			std::cout << "BlendFilter[" << width << "x" << height << "]: took "
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

#endif /* INCLUDE_MMPILOT_BLEND_H_ */
