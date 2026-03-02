/*
 * weight.h
 *
 *  Created on: Feb 11, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_WEIGHT_H_
#define INCLUDE_MMPILOT_WEIGHT_H_

#include <mmpilot/texture.h>
#include <mmpilot/opengl.h>
#include <mmpilot/render.h>
#include <mmpilot/util.h>

#include <memory>
#include <vector>
#include <iostream>


namespace mmpilot {

class WeightRadius {
public:
	float radius = -1;		// pixels

	std::shared_ptr<GL_Tex2D> out;

	void init(GLenum format, int width_, int height_)
	{
		if(have_init) {
			throw std::logic_error("already initialized");
		}
		width = width_;
		height = height_;

		if(radius < 0) {
			radius = width / 2;
		}

		std::string shader;
		switch(format) {
			case GL_RED: shader = "radius_r.glsl"; break;
			case GL_RG:  shader = "radius_rw.glsl"; break;
			default:
				throw std::logic_error("invalid format");
		}
		const auto vs = render::fullscreen_vertex_shader();
		const auto fs = GL_compile_shader(GL_FRAGMENT_SHADER, "shader/weight/" + shader);
		prog = GL_link_program(vs, fs);

		out = std::make_shared<GL_Tex2D>(width, height, GL_RG8, GL_RG, GL_UNSIGNED_BYTE);
		fbo = GL_create_FBO(out->id);

		have_init = true;
	}

	void exec(std::shared_ptr<GL_Tex2D> in)
	{
		if(!have_init) {
			init(in->format, in->width, in->height);
		}
		const auto begin = get_time_micros();

		glUseProgram(prog);

		GL_bind_tex(prog, "uSrc", in->id, 0);

		GL_uniform_2f(prog, "uCenter", width / 2., height / 2.);
		GL_uniform_1f(prog, "uRadiusSq", radius * radius);

		render::fullscreen(fbo, width, height);

		GL_finish("WeightRadius::exec()");

		std::cout << "WeightRadius[" << width << "x" << height << "]: took "
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

#endif /* INCLUDE_MMPILOT_WEIGHT_H_ */
