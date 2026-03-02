/*
 * affine.cpp
 *
 *  Created on: Feb 27, 2026
 *      Author: mad
 */

#include <mmpilot/affine.h>
#include <mmpilot/opengl.h>
#include <mmpilot/render.h>
#include <mmpilot/util.h>

#include <array>
#include <vector>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <cmath>
#include <iostream>


namespace mmpilot {

using Vec4 = Eigen::Matrix<double, 4, 1>;

static void assemble_equations(
		Vec4& G,
		Vec4& H,
		const std::vector<float>& G_buf,
		const std::vector<float>& H_buf
	)
{
	auto sumRGBA = [&](const std::vector<float>& v, double out[4]) {
		out[0] = out[1] = out[2] = out[3] = 0;
		const auto N = v.size() / 4;
		for(size_t i = 0; i < N; ++i) {
			out[0] += v[i * 4 + 0];
			out[1] += v[i * 4 + 1];
			out[2] += v[i * 4 + 2];
			out[3] += v[i * 4 + 3];
		}
	};

	double G_sum[4], H_sum[4];
	sumRGBA(G_buf, G_sum);
	sumRGBA(H_buf, H_sum);

	// Gradient g = sum(J^T r)
	for(int i = 0; i < 4; ++i) {
		G[i] = G_sum[i];
	}
	// Diagonal of H: sum(Jk^2)
	for(int i = 0; i < 4; ++i) {
		H[i] = H_sum[i];
	}
}

Affine::Params Affine::exec(
		std::shared_ptr<GL_Tex2D> ref, std::shared_ptr<GL_Tex2D> img, const Params& init_p)
{
	if(!have_init) {
		init(img->width, img->height);
	}
	if(img->width != width || img->height != height) {
		throw std::runtime_error("Affine::solve(): dimension mismatch");
	}
	const auto begin = get_time_micros();

	Params params = init_p;

	for(int iter = 0; iter < num_iters; ++iter)
	{
		// ---------- (1) Per-pixel J + residual
		glUseProgram(prog_jacobian);

		GL_bind_tex(prog_jacobian, "uRef", ref->id, 0);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		GL_bind_tex(prog_jacobian, "uImg", img->id, 1);

		GL_uniform_2f(prog_jacobian, "uCenter", width / 2.f, height / 2.f);
		GL_uniform_2f(prog_jacobian, "uInvSize", 1.f / width, 1.f / height);
		GL_uniform_fv(prog_jacobian, "uParams", params);

		render::fullscreen(fbo_jacobian, width, height);

		// ---------- (2) Reduce over Y chunks: G + diag(D)
		glUseProgram(prog_gradient);

		GL_bind_tex(prog_gradient, "uRes", tex_residual, 0);
		GL_bind_tex(prog_gradient, "uJ0", tex_jacobian, 1);

		GL_uniform_1i(prog_gradient, "uHeight", height);
		GL_uniform_1i(prog_gradient, "uChunkSize", reduction_chunk);

		render::fullscreen(fbo_gradient, width, height);

		GL_finish("Affine::solve()");

		// ---------- Read back partials
		GL_read_FBO_RGBA(fbo_gradient, 0, width, reduction_chunk, G_buf);
		GL_read_FBO_RGBA(fbo_gradient, 1, width, reduction_chunk, H_buf);
		GL_read_FBO_RGBA(fbo_gradient, 2, width, reduction_chunk, RwHxy_buf);

		// ---------- Assemble and solve
		Vec4 hessian;
		Vec4 gradient;

		assemble_equations(gradient, hessian, G_buf, H_buf);

		double R_sum = 0;
		double W_sum = 0;
		double H_xy = 0;
		for(size_t i = 0; i < RwHxy_buf.size() / 4; ++i) {
			R_sum += RwHxy_buf[i * 4 + 0];
			W_sum += RwHxy_buf[i * 4 + 1];
			H_xy  += RwHxy_buf[i * 4 + 2];
		}
		const float num_pixel = width * height;

		Mat2f A_xy;
		A_xy << hessian(0), H_xy,
				H_xy, hessian(1);

		params.R_norm = 1000 * sqrt(R_sum / W_sum);
		params.overlap = W_sum / num_pixel;
		params.H_xy = A_xy * 1000;

//		std::cout << "G = " << gradient.transpose() << std::endl;
//		std::cout << "H = " << hessian.transpose() << std::endl;

		// Apply damping
		hessian[0] += damping_xy * num_pixel;
		hessian[1] += damping_xy * num_pixel;
		hessian[2] += damping_yaw * num_pixel;
		hessian[3] += damping_scale * num_pixel;

		Vec4 delta = Vec4::Zero();
		for(int i = 0; i < 4; ++i) {
			if(hessian[i] > 0) {
				delta[i] = -gradient[i] / hessian[i];
			}
		}

		// Use 2x2 system to solve for dx, dy
		try {
			const Vec2f d = A_xy.colPivHouseholderQr().solve(Vec2f(gradient(0), gradient(1)));
			delta[0] = -d.x();
			delta[1] = -d.y();
		} catch(...) {
			// ignore
		}

		// Update params
		const auto step_norm = delta.norm();

//		std::cout << "iter " << iter << ": delta = " << delta.transpose() << std::endl;

		if(!std::isfinite(step_norm)) {
			break;
		}
		for(int i = 0; i < 4; ++i) {
			params[i] += delta[i];
		}
	}

//	std::cout << "Affine: R_norm = " << params.R_norm << ", overlap = " << params.overlap
//				<< ", H_xx = " << params.H_xy(0,0) << ", H_yy = " << params.H_xy(1,1)
//				<< ", H_xy = " << params.H_xy(0,1) << std::endl;

	if(debug) {
		glUseProgram(prog_debug);

		GL_bind_tex(prog_debug, "uImg", img->id, 0);
		GL_bind_tex(prog_debug, "uRes", tex_residual->id, 1);

		GL_uniform_2f(prog_debug, "uCenter", width / 2.f, height / 2.f);

		render::fullscreen(fbo_debug, width, height);
	}

	GL_finish("Affine::solve()");

	std::cout << "Affine[" << width << "x" << height << "]: took "
				<< (get_time_micros() - begin) / 1000.f << " ms" << std::endl;

	return params;
}

void Affine::init(int width_, int height_)
{
	if(have_init) {
		throw std::logic_error("already initialized");
	}
	width = width_;
	height = height_;

	auto fs_jacobian = GL_compile_shader(GL_FRAGMENT_SHADER, "shader/affine/jacobian.glsl");
	auto fs_gradient = GL_compile_shader(GL_FRAGMENT_SHADER, "shader/affine/gradient.glsl");
	auto fs_debug    = GL_compile_shader(GL_FRAGMENT_SHADER, "shader/debug/residual_overlay.glsl");

	const auto vs = render::get_fullscreen_vertex_shader();
	prog_jacobian = GL_link_program(vs, fs_jacobian);
	prog_gradient = GL_link_program(vs, fs_gradient);
	prog_debug  = GL_link_program(vs, fs_debug);

	glDeleteShader(fs_jacobian);
	glDeleteShader(fs_gradient);
	glDeleteShader(fs_debug);

	tex_residual = std::make_shared<GL_Tex2D>(width, height, GL_RG16F, GL_RG, GL_HALF_FLOAT);
	tex_jacobian = std::make_shared<GL_Tex2D>(width, height, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);

	tex_gradient = std::make_shared<GL_Tex2D>(width, reduction_chunk, GL_RGBA32F, GL_RGBA, GL_FLOAT);
	tex_hessian  = std::make_shared<GL_Tex2D>(width, reduction_chunk, GL_RGBA32F, GL_RGBA, GL_FLOAT);
	tex_RwHxy    = std::make_shared<GL_Tex2D>(width, reduction_chunk, GL_RGBA32F, GL_RGBA, GL_FLOAT);

	fbo_jacobian = GL_create_FBO({tex_jacobian, tex_residual});
	fbo_gradient = GL_create_FBO({tex_gradient, tex_hessian, tex_RwHxy});

	tex_debug = std::make_shared<GL_Tex2D>(width, height, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);
	fbo_debug = GL_create_FBO(tex_debug->id);

	have_init = true;
}

Affine::~Affine()
{
	glDeleteProgram(prog_jacobian);
	glDeleteProgram(prog_gradient);

	glDeleteFramebuffers(1, &fbo_jacobian);
	glDeleteFramebuffers(1, &fbo_gradient);
}


} // mmpilot
