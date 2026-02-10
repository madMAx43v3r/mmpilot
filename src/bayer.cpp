/*
 * bayer.cpp
 *
 *  Created on: Feb 10, 2026
 *      Author: mad
 */

#include <mmpilot/bayer.h>
#include <mmpilot/render.h>


namespace mmpilot {

void DeBayer::init(int width_, int height_, std::string format_)
{
	width = width_;
	height = height_;
	format = format_;

	if(format == "SBGGR16") {
		fs_luma = GL_compile_shader_file(GL_FRAGMENT_SHADER, "shader/bayer/BGGR16_luma.glsl");
		fs_rgba = GL_compile_shader_file(GL_FRAGMENT_SHADER, "shader/bayer/BGGR16_rgba.glsl");
		input = std::make_shared<GL_Tex2D>(width, height, GL_R16UI, GL_RED_INTEGER, GL_UNSIGNED_SHORT);
	} else {
		throw std::runtime_error("DeBayer: invalid format: " + format);
	}

	const auto vs = render::get_fullscreen_vertex_shader();
	prog_luma = GL_link_program(vs, fs_luma);
	prog_rgba = GL_link_program(vs, fs_rgba);

	if(on_luma) {
		out_luma = std::make_shared<GL_Tex2D>(width, height, GL_RG16F, GL_RG, GL_HALF_FLOAT);
		fbo_luma = GL_create_FBO(out_luma->id);
	}
	if(on_rgba) {
		out_rgba = std::make_shared<GL_Tex2D>(width, height, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);
		fbo_rgba = GL_create_FBO(out_rgba->id);
	}
	have_init = true;
}

void DeBayer::handle(std::shared_ptr<CameraFrame> frame)
{
	if(have_init) {
		if(frame->width != width || frame->height != height || frame->pixel_format != format) {
			throw std::runtime_error("DeBayer: frame dimensions / format mismatch");
		}
	} else {
		init(frame->width, frame->height, frame->pixel_format);
	}

	if(frame->data.size() != 1) {
		throw std::runtime_error("DeBayer: invalid frame data");
	}
	input->upload(frame->data[0].first, frame->stride / 2);

	if(out_luma) {
		glUseProgram(prog_luma);
		GL_bind_tex(prog_luma, "uBayer", input->id, 0);

		render::fullscreen(fbo_luma, width, height);

		GL_finish("DeBayer::handle(): prog_luma");
		on_luma(out_luma);
	}

	if(out_rgba) {
		glUseProgram(prog_rgba);
		GL_bind_tex(prog_rgba, "uBayer", input->id, 0);

		GL_set_uniform_1f(prog_rgba, "uBlack", black);
		GL_set_uniform_1f(prog_rgba, "uGain", gain);
		GL_set_uniform_1f(prog_rgba, "uGamma", gamma);

		render::fullscreen(fbo_rgba, width, height);

		GL_finish("DeBayer::handle(): prog_rgba");
		on_rgba(out_rgba);
	}
}


} // mmpilot
