/*
 * pyramid.h
 *
 *  Created on: Feb 12, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_PYRAMID_H_
#define INCLUDE_MMPILOT_PYRAMID_H_

#include <mmpilot/texture.h>
#include <mmpilot/opengl.h>
#include <mmpilot/render.h>
#include <mmpilot/util.h>

#include <memory>
#include <vector>
#include <iostream>


namespace mmpilot {

class PyramidFilter {
public:
	int depth = 4;

	std::vector<std::shared_ptr<GL_Tex2D>> out;

	void init(int width_, int height_, GLenum int_format, GLenum format, GLenum type)
	{
		if(have_init) {
			throw std::logic_error("already initialized");
		}
		width = width_;
		height = height_;

		if(depth < 1) {
			throw std::logic_error("invalid depth");
		}
		out.resize(1);

		std::string shader;
		switch(format) {
			case GL_RED:  shader = "downscale_r.glsl"; break;
			case GL_RG:   shader = "downscale_rg.glsl"; break;
			case GL_RGBA: shader = "downscale_rgba.glsl"; break;
			default:
				throw std::runtime_error("PyramidFilter: invalid format");
		}
		const auto vs = render::get_fullscreen_vertex_shader();
		const auto fs = GL_compile_shader(GL_FRAGMENT_SHADER, "shader/scale/" + shader);
		prog = GL_link_program(vs, fs);

		int w = width;
		int h = height;
		for(int i = 1; i < depth; ++i) {
			w /= 2; h /= 2;
			auto tex = std::make_shared<GL_Tex2D>(w, h, int_format, format, type);
			fbo.push_back(GL_create_FBO(tex->id));
			out.push_back(tex);
		}
		have_init = true;
	}

	void exec(std::shared_ptr<GL_Tex2D> in)
	{
		if(!have_init) {
			init(in->width, in->height, in->internal_fmt, in->format, in->type);
		}
		const auto begin = get_time_micros();

		out[0] = in;

		int w = width;
		int h = height;
		for(int i = 1; i < depth; ++i) {
			glUseProgram(prog);

			GL_bind_tex(prog, "uSrc", out[i-1]->id, 0);

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

			GL_uniform_2f(prog, "uInvSrcSize", 1.f / w, 1.f / h);

			w /= 2; h /= 2;

			render::fullscreen(fbo[i-1], w, h);
		}
		GL_finish("PyramidFilter::exec()");

		std::cerr << "PyramidFilter[" << width << "x" << height << "]: took "
				<< (get_time_micros() - begin) / 1000.f << " ms" << std::endl;
	}

private:
	int width = 0;
	int height = 0;

	GLuint prog = 0;

	std::vector<GLuint> fbo;

	bool have_init = false;

};


} // mmpilot

#endif /* INCLUDE_MMPILOT_PYRAMID_H_ */
