/*
 * virtual_cam.h
 *
 *  Created on: Feb 16, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_VIRTUAL_CAM_H_
#define INCLUDE_MMPILOT_VIRTUAL_CAM_H_

#include <mmpilot/texture.h>
#include <mmpilot/opengl.h>
#include <mmpilot/render.h>
#include <mmpilot/util.h>
#include <mmpilot/math.h>

#include <memory>
#include <vector>
#include <iostream>


namespace mmpilot {

class VirtualCam {
public:
	int width = 1024;		// output size
	int height = 1024;		// output size

	float FOV_in = 180;		// fisheye FOV in degrees (diagonal)
	float FOV_cam = 120;	// pinhole FOV in degrees (diagonal)

	float FOV_circle = 1;		// correction for black corners

	float K2 = 0;			// r^2 coeff on angle-radius (r = theta)
	float K4 = 0;			// r^4 coeff on angle-radius

	float f_in = 0;			// focal length (pixels)
	float f_cam = 0;		// focal length (pixels)

	bool enable_src_pos = false;

	Mat3f R_mat = Mat3f::Identity();	// virtual cam rotation matrix

	std::shared_ptr<GL_Tex2D> out;
	std::shared_ptr<GL_Tex2D> tex_src_pos;		// pixel coords

	void init(GLenum int_format, GLenum format, GLenum type)
	{
		if(have_init) {
			throw std::logic_error("already initialized");
		}
		// rectilinear baseline
		const auto diag = Vec2f(width, height).norm() / 2;
		f_cam = diag / std::tan(deg2rad(FOV_cam) / 2);

		const auto vs = render::get_fullscreen_vertex_shader();
		const auto fs = GL_compile_shader(GL_FRAGMENT_SHADER, "shader/rectify/virtual_cam.glsl");
		prog = GL_link_program(vs, fs);

		out = std::make_shared<GL_Tex2D>(width, height, int_format, format, type);

		if(enable_src_pos) {
			tex_src_pos = std::make_shared<GL_Tex2D>(width, height, GL_RGBA32F, GL_RGBA, GL_FLOAT);
			fbo = GL_create_FBO({out->id, tex_src_pos->id});
		} else {
			fbo = GL_create_FBO(out->id);
		}
		have_init = true;
	}

	void exec(std::shared_ptr<GL_Tex2D> in)
	{
		if(!have_init) {
			init(in->internal_fmt, in->format, in->type);
		}
		const auto begin = get_time_micros();

		// fisheye baseline (equidistant)
		const auto diag = FOV_circle * Vec2f(in->width, in->height).norm() / 2;
		f_in = diag / (deg2rad(FOV_in) / 2);
		{
			const auto r = deg2rad(FOV_in) / 2;
			const auto r2 = r * r;
			const auto r4 = r2 * r2;
			const auto scale = (r + K2 * r2 + K4 * r4) / r;
			f_in /= scale;
			std::cout << "VirtualCam: distortion factor = " << scale << std::endl;
		}
		glUseProgram(prog);

		GL_bind_tex(prog, "uSrc", in->id, 0);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

		GL_uniform_2f(prog, "uInvSrcSize", 1.f / in->width, 1.f / in->height);
		GL_uniform_2f(prog, "uCenter", width / 2.f, height / 2.f);
		GL_uniform_2f(prog, "uInvF", 1 / f_cam, 1 / f_cam);
		GL_uniform_mat3(prog, "uRot", R_mat.data());

		GL_uniform_1f(prog, "uF", f_in);
		GL_uniform_1f(prog, "uK2", K2);
		GL_uniform_1f(prog, "uK4", K4);

		render::fullscreen(fbo, width, height);

		GL_finish("VirtualCam::exec()");

		std::cout << "VirtualCam[" << width << "x" << height << "]: took "
				<< (get_time_micros() - begin) / 1000.f << " ms" << std::endl;
	}

private:
	GLuint fbo = 0;
	GLuint prog = 0;

	bool have_init = false;

};


} // mmpilot

#endif /* INCLUDE_MMPILOT_VIRTUAL_CAM_H_ */
