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

#include <memory>
#include <vector>


namespace mmpilot {

class WeightRadius {
public:
	float radius = -1;

	std::shared_ptr<GL_Tex2D> out;

	std::function<void(std::shared_ptr<GL_Tex2D>)> next;

	void init(int width_, int height_)
	{
		width = width_;
		height = height_;

		if(radius < 0) {
			radius = width / 2;
		}
		const auto vs = render::get_fullscreen_vertex_shader();
		const auto fs = GL_compile_shader_file(GL_FRAGMENT_SHADER, "shader/weight/radius.glsl");
		prog = GL_link_program(vs, fs);

		out = std::make_shared<GL_Tex2D>(width, height, GL_RG8, GL_RG, GL_UNSIGNED_BYTE);
		fbo = GL_create_FBO(out->id);

		have_init = true;
	}

	void handle(std::shared_ptr<GL_Tex2D> in)
	{
		if(have_init) {
			if(in->width != width || in->height != height) {
				throw std::runtime_error("WeightRadius: dimension mismatch");
			}
		} else {
			init(in->width, in->height);
		}
		glUseProgram(prog);
		GL_bind_tex(prog, "uSrc", in->id, 0);

		GL_set_uniform_2f(prog, "uCenter", width / 2, height / 2);
		GL_set_uniform_1f(prog, "uRadiusSq", radius * radius);

		render::fullscreen(fbo, width, height);

		GL_finish("WeightRadius::handle()");

		if(next) {
			next(out);
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

#endif /* INCLUDE_MMPILOT_WEIGHT_H_ */
