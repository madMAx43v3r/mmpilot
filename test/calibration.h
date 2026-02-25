/*
 * calibration.h
 *
 *  Created on: Feb 21, 2026
 *      Author: mad
 */

#ifndef TEST_CALIBRATION_H_
#define TEST_CALIBRATION_H_

#include <mmpilot/calib.h>

#include "pipeline.h"


class CalibrationPipe : public Pipeline {
public:
	float trigger_delta = 25;		// pixels traveled
	float trigger_scale = 1.25;		// relative scale change
	float trigger_angle = 10;		// [deg]

	int reduction_chunk = 32;

protected:
	GradientFilter in_gradient;

	std::shared_ptr<GL_Tex2D> base_grad;
	std::shared_ptr<GL_Tex2D> base_src_pos;

	std::shared_ptr<GL_Tex2D> tex_jacobian[2];
	std::shared_ptr<GL_Tex2D> tex_gradient;

	void init(int width, int height) override
	{
		is_fisheye = true;
		virtual_cam.enable_src_pos = true;

		Pipeline::init(width, height);

		in_gradient.init(in_width, in_height);

		base_grad    = std::make_shared<GL_Tex2D>(in_width, in_height, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
		base_src_pos = std::make_shared<GL_Tex2D>(src_width, src_height, GL_RGBA32F, GL_RGBA, GL_FLOAT);

		tex_jacobian[0] = std::make_shared<GL_Tex2D>(src_width, src_height, GL_RG32F, GL_RG, GL_FLOAT);
		tex_jacobian[1] = std::make_shared<GL_Tex2D>(src_width, src_height, GL_RG32F, GL_RG, GL_FLOAT);
		tex_gradient = std::make_shared<GL_Tex2D>(src_width, reduction_chunk, GL_RGBA32F, GL_RGBA, GL_FLOAT);

		const auto vs = render::get_fullscreen_vertex_shader();
		const auto fs_jacobian = GL_compile_shader(GL_FRAGMENT_SHADER, "shader/rectify/jacobian.glsl");
		const auto fs_gradient = GL_compile_shader(GL_FRAGMENT_SHADER, "shader/rectify/gradient.glsl");
		prog_jacobian = GL_link_program(vs, fs_jacobian);
		prog_gradient = GL_link_program(vs, fs_gradient);

		fbo_jacobian[0] = GL_create_FBO(tex_jacobian[0]->id);
		fbo_jacobian[1] = GL_create_FBO(tex_jacobian[1]->id);
		fbo_gradient = GL_create_FBO(tex_gradient->id);
	}

	void rebase() override
	{
		Pipeline::rebase();

		base_gyro = gyro_state;

		GL_blit(base_grad, in_gradient.out);
		GL_blit(base_src_pos, virtual_cam.tex_src_pos);
	}

	void exec_filter(std::shared_ptr<GL_Tex2D> input) override
	{
		Pipeline::exec_filter(input);

		in_gradient.exec(weight_radius.out);
	}

	void distortion()
	{
		glUseProgram(prog_jacobian);

		GL_uniform_1f(prog_jacobian, "uF", virtual_cam.f_in);
		GL_uniform_2f(prog_jacobian, "uInvSrcSize", 1.f / in_width, 1.f / in_height);

		// ---------- (1) Per-pixel J0
		GL_bind_tex(prog_jacobian, "uSrcPos", base_src_pos->id, 0);

		GL_bind_tex(prog_jacobian, "uSrcGrad", base_grad->id, 1);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		render::fullscreen(fbo_jacobian[0], src_width, src_height);

		// ---------- (1) Per-pixel J1
		GL_bind_tex(prog_jacobian, "uSrcPos", virtual_cam.tex_src_pos->id, 0);

		GL_bind_tex(prog_jacobian, "uSrcGrad", in_gradient.out->id, 1);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		render::fullscreen(fbo_jacobian[1], src_width, src_height);

		// ---------- (2) Reduce over Y chunks: G + diag(D)
		glUseProgram(prog_gradient);

		GL_bind_tex(prog_gradient, "uRes", stage[0]->solver.tex_residual->id, 0);
		GL_bind_tex(prog_gradient, "uJ0", tex_jacobian[0]->id, 1);
		GL_bind_tex(prog_gradient, "uJ1", tex_jacobian[1]->id, 2);

		GL_uniform_1i(prog_gradient, "uHeight", src_height);
		GL_uniform_1i(prog_gradient, "uChunkSize", reduction_chunk);

		render::fullscreen(fbo_gradient, src_width, src_height);

		GL_finish("run()");

		// ---------- Read back partials
		GL_read_FBO_RGBA(fbo_gradient, 0, src_width, reduction_chunk, GD_buf);

		Vec2d G_sum = Vec2d::Zero();
		Vec2d H_sum = Vec2d::Zero();
		for(size_t i = 0; i < GD_buf.size() / 4; ++i) {
			G_sum += Vec2d(GD_buf[i * 4 + 0], GD_buf[i * 4 + 1]);
			H_sum += Vec2d(GD_buf[i * 4 + 2], GD_buf[i * 4 + 3]);
		}
		gradient += G_sum;
		hessian  += H_sum;

//		std::cout << "G = " << gradient.transpose() << std::endl;
//		std::cout << "H = " << hessian.transpose() << std::endl;

		Vec2d delta = Vec2d::Zero();
		for(int i = 0; i < 2; ++i) {
			if(hessian[i] > 0) {
				delta[i] = -gradient[i] / hessian[i];
			}
//			if(H_sum[i] > 0) {
//				delta[i] = -G_sum[i] / H_sum[i];
//			}
		}
//		virtual_cam.K2 += delta[0];
//		virtual_cam.K4 += delta[1];

		std::cout << "calib: K2/K4 delta = " << delta.transpose() << std::endl;
//		std::cout << "calib: K2 = " << virtual_cam.K2 << ", K4 = " << virtual_cam.K4 << std::endl;
	}

	void extrinsic(const calib::ExtrinsicSample& sample)
	{
		extr_samples.push_back(sample);

		Mat3d R_BC = this->R_BC.cast<double>();

		for(int iter = 0; iter < 10; ++iter)
		{
			const auto f_cam = virtual_cam.f_cam;
			const auto delta = calib::solve_delta_extrinsic(R_BC, extr_samples, f_cam, f_cam, 1e-3);
			calib::apply_delta_rot(R_BC, -delta);

			const auto rpy = rot_zyx_to_rpy_deg(R_BC);

			std::cout << "calib: iter " << iter << ": RPY(R_BC) = " << rpy.transpose() << " deg" << std::endl;
		}
	}

	void update() override
	{
		const auto H = get_params();
		const auto T = H.transform();

		const Vec3f delta_rpy = gyro_state.get_rpy() - base_gyro.get_rpy();
		const float roll_pitch = Vec2f(delta_rpy.x(), delta_rpy.y()).norm();

//		std::cout << "calib: roll_pitch = " << roll_pitch << " deg" << std::endl;

//		if(roll_pitch > trigger_angle || T.pos.norm() > trigger_delta
//			|| T.scale > trigger_scale || T.scale < 1 / trigger_scale)
		{
			if(roll_pitch > trigger_angle)
			{
				distortion();

				if(T.pos.norm() < trigger_delta)
				{
					calib::ExtrinsicSample sample;
					sample.dR_B = base_gyro.matrix().transpose() * gyro_state.matrix();
					sample.trans = T.pos;

					std::cout << "calib: sample: trans = " << sample.trans.transpose() << std::endl;

					extrinsic(sample);
				}
			}

			total_shift += T.pos.norm();
			std::cout << "total_shift[" << counter << "] = " << total_shift << std::endl;
			counter++;

			rebase();
		}

//		show(display, flip_image.out, {1, 0.2, 1, 1});
//		show(display, virtual_cam.out, {1, 0.1, 1, 1});
//		show(display, pyramid_filter.out[4], {1, 0.5, 1, 1});
//		show(display, stage[3]->smooth[1].out, {1, 0.1, 1, 1});
//		show(display, stage[0]->base_img, {0, 1, 1, 1});
		show(display, stage[0]->solver.tex_debug, {1, 1, 1, 1});
	}

private:
	Vec2d gradient = Vec2d::Zero();
	Vec2d hessian = Vec2d::Zero();

	Gyro::State base_gyro;

	std::vector<calib::ExtrinsicSample> extr_samples;

	int counter = 0;
	float total_shift = 0;

	std::vector<float> GD_buf;

	GLuint prog_jacobian = 0;
	GLuint prog_gradient = 0;

	GLuint fbo_jacobian[2] = {};
	GLuint fbo_gradient = 0;

};



#endif /* TEST_CALIBRATION_H_ */
