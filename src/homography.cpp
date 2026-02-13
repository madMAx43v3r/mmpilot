/*
 * homography.cpp
 *
 *  Created on: Feb 5, 2026
 *      Author: mad
 */

// Host-side Gauss-Newton loop for your three ES 3.1 fragment kernels:
//
//   (1) Per-pixel J + residual (MRT: J0, J1, Rw, UV)
//   (2) Vertical chunk reduction: G (J^T r) + D (diag of J^T J)
//   (3) Vertical chunk reduction: off-diagonals of J^T J
//
// It assembles the 8x8 normal equations on CPU using Eigen and solves:
//
//     H * delta = -g
//     p <- p + delta
//
// Assumptions:
// - You already have a valid OpenGL ES 3.1 context current (EGL/GLFW/etc).
// - uRef (R16F) and uGrad (RGBA16F = Y,Ix,Iy,w) textures are created + filled.
// - Your shaders compile as fragment shaders and are drawn over fullscreen.
// - For simplicity, final reduction over X and over "chunk rows" is done on CPU
//   by reading back the (W x chunkSize) partial textures (manageable).
//
// Build hint (Linux):
//   g++ -O3 -std=c++20 homography_gn_host.cpp -lEGL -lGLESv2 -I /usr/include/eigen3
//
// NOTE: Use glGetError / KHR_debug in your real code.

#include <mmpilot/homography.h>
#include <mmpilot/opengl.h>
#include <mmpilot/render.h>
#include <mmpilot/util.h>

#include <Eigen/Dense>

#include <array>
#include <vector>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <cmath>
#include <iostream>


namespace mmpilot {

using Mat3 = Eigen::Matrix<double, 3, 3>;
using Mat8 = Eigen::Matrix<double, 8, 8, Eigen::RowMajor>;
using Vec8 = Eigen::Matrix<double, 8, 1>;

// ------------------------ Solver Assembly ------------------------

static void assemble_equations(
		Vec8& g,
		Mat8& H,
		const std::vector<float>& G0_rgba, // b0..b3
		const std::vector<float>& G1_rgba, // b4..b7
		const std::vector<float>& D0_rgba, // d0..d3
		const std::vector<float>& D1_rgba, // d4..d7
		const std::array<std::vector<float>, 7>& S_rgba)
{
	g.setZero();
	H.setZero();

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

	double b0_3[4], b4_7[4], d0_3[4], d4_7[4];
	sumRGBA(G0_rgba, b0_3);
	sumRGBA(G1_rgba, b4_7);
	sumRGBA(D0_rgba, d0_3);
	sumRGBA(D1_rgba, d4_7);

	// Gradient g = sum(J^T r). Your shader accumulates G += J * R.
	// Normal equations: H * delta = -g.
	for(int i = 0; i < 4; ++i) {
		g(i)		= b0_3[i];
		g(i + 4)	= b4_7[i];
	}

	// Diagonal of H: sum(Jk^2)
	for(int i = 0; i < 4; ++i) {
		H(i, i)			= d0_3[i];
		H(i + 4, i + 4)	= d4_7[i];
	}

	// Off-diagonals mapping from your MRT packing:
	// S0: (0,1),(0,2),(0,3),(0,4)
	// S1: (0,5),(0,6),(0,7),(1,2)
	// S2: (1,3),(1,4),(1,5),(1,6)
	// S3: (1,7),(2,3),(2,4),(2,5)
	// S4: (2,6),(2,7),(3,4),(3,5)
	// S5: (3,6),(3,7),(4,5),(4,6)
	// S6: (4,7),(5,6),(5,7),(6,7)
	double s[7][4];
	for(int i = 0; i < 7; ++i) {
		sumRGBA(S_rgba[i], s[i]);
	}

	auto setH = [&](int i, int j, double v) {
		H(i, j) = v;
		H(j, i) = v;
	};

//	setH(0, 1, s[0][0]);
//	setH(0, 2, s[0][1]);
//	setH(0, 3, s[0][2]);
//	setH(0, 4, s[0][3]);
//	setH(0, 5, s[1][0]);
//	setH(0, 6, s[1][1]);
//	setH(0, 7, s[1][2]);
//	setH(1, 2, s[1][3]);
//	setH(1, 3, s[2][0]);
//	setH(1, 4, s[2][1]);
//	setH(1, 5, s[2][2]);
//	setH(1, 6, s[2][3]);
//	setH(1, 7, s[3][0]);
//	setH(2, 3, s[3][1]);
//	setH(2, 4, s[3][2]);
//	setH(2, 5, s[3][3]);
//	setH(2, 6, s[4][0]);
//	setH(2, 7, s[4][1]);
//	setH(3, 4, s[4][2]);
//	setH(3, 5, s[4][3]);
//	setH(3, 6, s[5][0]);
//	setH(3, 7, s[5][1]);
//	setH(4, 5, s[5][2]);
//	setH(4, 6, s[5][3]);
//	setH(4, 7, s[6][0]);
//	setH(5, 6, s[6][1]);
//	setH(5, 7, s[6][2]);
//	setH(6, 7, s[6][3]);
}

void apply_damping(Mat8& H, float lambda)
{
	for(int i = 0; i < 8; ++i) {
		H(i, i) = H(i, i) * lambda + 1;
	}
}

// ------------------------ Main entry for one solve ------------------------

Homography::Params8 Homography::solve(std::shared_ptr<GL_Tex2D> ref, std::shared_ptr<GL_Tex2D> img)
{
	Params8 p = {};
	p[0] = 1;
	p[4] = 1;
	return solve(ref, img, p);
}

Homography::Params8 Homography::solve(
		std::shared_ptr<GL_Tex2D> ref, std::shared_ptr<GL_Tex2D> img, const Params8& init_p)
{
	if(!have_init) {
		init(img->width, img->height);
	}
	if(img->width != width || img->height != height) {
		throw std::runtime_error("Homography::solve(): dimension mismatch");
	}
	const auto begin = get_time_micros();

	Params8 params = init_p;

	// CPU readback buffers
	std::vector<float> G0_rgba, G1_rgba, D0_rgba, D1_rgba;
	std::array<std::vector<float>, 7> S_rgba;

	for(int iter = 0; iter < num_iters; ++iter)
	{
		// ---------- (1) Per-pixel J + residual
		glUseProgram(prog_jacobian);

		GL_bind_tex(prog_jacobian, "uRef", ref->id, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		GL_bind_tex(prog_jacobian, "uImg", img->id, 1);

		GL_uniform_2f(prog_jacobian, "uCenter", width / 2, height / 2);
		GL_uniform_2f(prog_jacobian, "uInvSize", 1.f / width, 1.f / height);
		GL_uniform_fv(prog_jacobian, "uParams", params);

		render::fullscreen(fbo_jacobian, width, height);

		// ---------- (2) Reduce over Y chunks: G + diag(D)
		glUseProgram(prog_gradient);

		GL_bind_tex(prog_gradient, "uRes", tex_residual->id, 0);
		GL_bind_tex(prog_gradient, "uJ0", tex_jacobian[0]->id, 1);
		GL_bind_tex(prog_gradient, "uJ1", tex_jacobian[1]->id, 2);

		GL_uniform_1i(prog_gradient, "uHeight", height);
		GL_uniform_1i(prog_gradient, "uChunkSize", reduction_chunk);

		render::fullscreen(fbo_gradient, width, height);

		// ---------- (3) Reduce off-diagonals
//		glUseProgram(prog_hessian);
//
//		GL_bind_tex(prog_hessian, "uJ0", tex_jacobian[0]->id, 0);
//		GL_bind_tex(prog_hessian, "uJ1", tex_jacobian[1]->id, 1);
//
//		GL_uniform_1i(prog_hessian, "uHeight", height);
//		GL_uniform_1i(prog_hessian, "uChunkSize", reduction_chunk);
//
//		render::fullscreen(fbo_hessian, width, height);

		// Ensure rendering finished before readback (simple path).
		GL_finish("Homography::solve()");

		// ---------- Read back partials (W * chunkSize pixels)
		GL_read_FBO_RGBA(fbo_gradient, 0, width, reduction_chunk, G0_rgba);
		GL_read_FBO_RGBA(fbo_gradient, 1, width, reduction_chunk, G1_rgba);
		GL_read_FBO_RGBA(fbo_gradient, 2, width, reduction_chunk, D0_rgba);
		GL_read_FBO_RGBA(fbo_gradient, 3, width, reduction_chunk, D1_rgba);

//		GL_read_FBO_RGBA(fbo_hessian, 0, width, reduction_chunk, S_rgba[0]);
//		GL_read_FBO_RGBA(fbo_hessian, 1, width, reduction_chunk, S_rgba[1]);
//		GL_read_FBO_RGBA(fbo_hessian, 2, width, reduction_chunk, S_rgba[2]);
//		GL_read_FBO_RGBA(fbo_hessian, 3, width, reduction_chunk, S_rgba[3]);
//		GL_read_FBO_RGBA(fbo_hessian, 4, width, reduction_chunk, S_rgba[4]);
//		GL_read_FBO_RGBA(fbo_hessian, 5, width, reduction_chunk, S_rgba[5]);
//		GL_read_FBO_RGBA(fbo_hessian, 6, width, reduction_chunk, S_rgba[6]);

		// ---------- Assemble and solve
		Mat8 hessian;
		Vec8 gradient;

		assemble_equations(gradient, hessian, G0_rgba, G1_rgba, D0_rgba, D1_rgba, S_rgba);

//		std::cerr << "G = " << std::endl << gradient.transpose() << std::endl;
//		std::cerr << "H = " << std::endl << hessian.diagonal().transpose() << std::endl;

		apply_damping(hessian, damping);

		// Extra damping for projective params
		hessian(6, 6) *= 10;
		hessian(7, 7) *= 10;

		// Solve H * delta = -g
		Eigen::LDLT<Mat8> ldlt(hessian);
		if(ldlt.info() != Eigen::Success) {
			std::cerr << "G = " << std::endl << gradient.transpose() << std::endl;
			std::cerr << "H = " << std::endl << hessian.diagonal().transpose() << std::endl;
			throw std::logic_error("LDLT failed");
		}
		const Vec8 delta = ldlt.solve(-gradient);

		// Update params
		const auto step_norm = delta.norm();

		std::cerr << "iter " << iter << ": delta = " << step_norm << std::endl;

		if(!std::isfinite(step_norm)) {
			throw std::logic_error("solver failed");
		}
		for(int i = 0; i < 8; ++i) {
			params[i] += delta[i];
		}

//		auto params_to_Hc = [&](const Params8& p) {
//			Mat3 H;
//			H << p[0], p[1], p[2],
//				 p[3], p[4], p[5],
//				 p[6], p[7], 1.0;
//			return H;
//		};
//
//		auto Hc_to_params = [&](const Mat3& H) {
//			// Normalize so H(2,2)=1
//			const auto s = H(2,2);
//			const auto N = H / s;
//			Params8 p;
//			p[0] = N(0,0); p[1] = N(0,1); p[2] = N(0,2);
//			p[3] = N(1,0); p[4] = N(1,1); p[5] = N(1,2);
//			p[6] = N(2,0); p[7] = N(2,1);
//			return p;
//		};
//
//		Mat3 Hc = params_to_Hc(params);
//
//		Mat3 dH;
//		dH << 1.0 + delta[0], delta[1],       delta[2],
//			  delta[3],       1.0 + delta[4], delta[5],
//			  delta[6],       delta[7],       1.0;
//
//		// Left-multiply composition
//		Hc = dH * Hc;
//
//		params = Hc_to_params(Hc);

		// renormalize homography scale to keep numbers sane
		// Here, keep p2 / p5 roughly on the same scale by normalizing by (p6,p7,1) magnitude.
		const auto s = std::sqrt(params[6]*params[6] + params[7]*params[7] + 1);
		for(auto& v : params) {
			// TODO: needed?
//			v /= s;
		}
	}
	std::cerr << "Homography[" << width << "x" << height << "]: took "
				<< (get_time_micros() - begin) / 1000.f << " ms" << std::endl;
	return params;
}

void Homography::init(int width_, int height_)
{
	if(have_init) {
		throw std::logic_error("already initialized");
	}
	width = width_;
	height = height_;

	auto fs_jacobian = GL_compile_shader(GL_FRAGMENT_SHADER, "shader/homographic/jacobian.glsl");
	auto fs_gradient = GL_compile_shader(GL_FRAGMENT_SHADER, "shader/homographic/gradient.glsl");
	auto fs_hessian  = GL_compile_shader(GL_FRAGMENT_SHADER, "shader/homographic/hessian.glsl");

	const auto vs = render::get_fullscreen_vertex_shader();
	prog_jacobian = GL_link_program(vs, fs_jacobian);
	prog_gradient = GL_link_program(vs, fs_gradient);
	prog_hessian  = GL_link_program(vs, fs_hessian);

	glDeleteShader(fs_jacobian);
	glDeleteShader(fs_gradient);
	glDeleteShader(fs_hessian);

	tex_uv       = std::make_shared<GL_Tex2D>(width, height, GL_RG16F, GL_RG, GL_HALF_FLOAT);
	tex_residual = std::make_shared<GL_Tex2D>(width, height, GL_RG16F, GL_RG, GL_HALF_FLOAT);

	for(int i = 0; i < 2; ++i) {
		tex_jacobian[i] = std::make_shared<GL_Tex2D>(width, height, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
		tex_gradient[i] = std::make_shared<GL_Tex2D>(width, reduction_chunk, GL_RGBA32F, GL_RGBA, GL_FLOAT);
	}
	// TODO: only need 2
	for(int i = 0; i < 9; ++i) {
		tex_hessian.push_back(
				std::make_shared<GL_Tex2D>(width, reduction_chunk, GL_RGBA32F, GL_RGBA, GL_FLOAT));
	}

	fbo_jacobian = GL_create_FBO(
			{tex_jacobian[0]->id, tex_jacobian[1]->id, tex_residual->id, tex_uv->id});
	fbo_gradient = GL_create_FBO(
			{tex_gradient[0]->id, tex_gradient[1]->id, tex_hessian[0]->id, tex_hessian[1]->id});

	std::vector<GLuint> hessian_off;
	for(size_t i = 2; i < tex_hessian.size(); ++i) {
		hessian_off.push_back(tex_hessian[i]->id);
	}
	if(hessian_off.size()) {
		fbo_hessian = GL_create_FBO(hessian_off);
	}

	have_init = true;
}

Homography::~Homography()
{
	glDeleteProgram(prog_jacobian);
	glDeleteProgram(prog_gradient);
	glDeleteProgram(prog_hessian);

	glDeleteFramebuffers(1, &fbo_jacobian);
	glDeleteFramebuffers(1, &fbo_gradient);
	glDeleteFramebuffers(1, &fbo_hessian);
}


} // mmpilot
