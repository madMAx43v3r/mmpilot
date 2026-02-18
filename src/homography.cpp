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

#include <array>
#include <vector>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <cmath>
#include <iostream>


namespace mmpilot {

using Vec8 = Eigen::Matrix<double, 8, 1>;

static void assemble_equations(
		Vec8& g,
		Vec8& H,
		const std::vector<float>& G0_rgba, // b0..b3
		const std::vector<float>& G1_rgba, // b4..b7
		const std::vector<float>& D0_rgba, // d0..d3
		const std::vector<float>& D1_rgba  // d4..d7
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
		H(i)		= d0_3[i];
		H(i + 4)	= d4_7[i];
	}
}

Transform2D Homography::Params::transform() const
{
	// Convert homography to affine transform
	Transform2D out;
	out.pos = translation();

	const Mat2f A = rotation();
	Eigen::JacobiSVD<Mat2f> svd(A, Eigen::ComputeFullU | Eigen::ComputeFullV);
	Mat2f U = svd.matrixU();
	Mat2f V = svd.matrixV();
	Vec2f svals = svd.singularValues();

	Mat2f R = U * V.transpose();
	if(R.determinant() < 0.0f) {      // reflection fix
		U.col(1) *= -1.0f;
		R = U * V.transpose();
	}
	out.rot = R;

	out.scale = (svals(0) + svals(1)) / 2;
//	out.scale = std::sqrt(std::abs(A.determinant()));
	return out;
}

Homography::Params Homography::solve(std::shared_ptr<GL_Tex2D> ref, std::shared_ptr<GL_Tex2D> img)
{
	Params p = {};
	p[0] = 1;
	p[4] = 1;
	return solve(ref, img, p);
}

Homography::Params Homography::solve(
		std::shared_ptr<GL_Tex2D> ref, std::shared_ptr<GL_Tex2D> img, const Params& init_p)
{
	if(!have_init) {
		init(img->width, img->height);
	}
	if(img->width != width || img->height != height) {
		throw std::runtime_error("Homography::solve(): dimension mismatch");
	}
	const auto begin = get_time_micros();

	Params params = init_p;

	// CPU readback buffers
	std::vector<float> G0_rgba, G1_rgba, D0_rgba, D1_rgba, RwHxy_rgba;

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

		GL_bind_tex(prog_gradient, "uRes", tex_residual->id, 0);
		GL_bind_tex(prog_gradient, "uJ0", tex_jacobian[0]->id, 1);
		GL_bind_tex(prog_gradient, "uJ1", tex_jacobian[1]->id, 2);

		GL_uniform_1i(prog_gradient, "uHeight", height);
		GL_uniform_1i(prog_gradient, "uChunkSize", reduction_chunk);

		render::fullscreen(fbo_gradient, width, height);

		GL_finish("Homography::solve()");

		// ---------- Read back partials (W * chunkSize pixels)
		GL_read_FBO_RGBA(fbo_gradient, 0, width, reduction_chunk, G0_rgba);
		GL_read_FBO_RGBA(fbo_gradient, 1, width, reduction_chunk, G1_rgba);
		GL_read_FBO_RGBA(fbo_gradient, 2, width, reduction_chunk, D0_rgba);
		GL_read_FBO_RGBA(fbo_gradient, 3, width, reduction_chunk, D1_rgba);
		GL_read_FBO_RGBA(fbo_gradient, 4, width, reduction_chunk, RwHxy_rgba);

		// ---------- Assemble and solve
		Vec8 hessian;
		Vec8 gradient;

		assemble_equations(gradient, hessian, G0_rgba, G1_rgba, D0_rgba, D1_rgba);

		double R_sum = 0;
		double W_sum = 0;
		double H_xy = 0;
		double H_67 = 0;
		for(size_t i = 0; i < RwHxy_rgba.size() / 4; ++i) {
			R_sum += RwHxy_rgba[i * 4 + 0];
			W_sum += RwHxy_rgba[i * 4 + 1];
			H_xy  += RwHxy_rgba[i * 4 + 2];
			H_67  += RwHxy_rgba[i * 4 + 3];
		}
		const float R_norm = 100 * sqrt(R_sum) / W_sum;
		const float num_pixel = width * height;

		params.R_norm = R_norm;
		params.overlap = W_sum / num_pixel;
		params.H_xy << hessian(2), H_xy, H_xy, hessian(5);
		params.H_xy *= 100 / num_pixel;

//		std::cout << "G = " << std::endl << gradient.transpose() << std::endl;
//		std::cout << "H = " << hessian.transpose() << std::endl;

		// Apply damping
		for(int i = 0; i < 8; ++i) {
			hessian(i) = hessian(i) * damping + 1;
		}

		// Extra damping for projective params
		hessian(6) *= proj_damping;
		hessian(7) *= proj_damping;

		// TODO: use H_xy + H_67

		Vec8 delta;
		for(int i = 0; i < 8; ++i) {
			delta[i] = gradient[i] / hessian[i];
		}

		// Update params
		const auto step_norm = delta.norm();

//		std::cout << "iter " << iter << ": delta = " << delta.transpose() << std::endl;

		if(!std::isfinite(step_norm)) {
			throw std::logic_error("Homography: solver failed");
		}
		for(int i = 0; i < 8; ++i) {
			params[i] -= delta[i];
		}
	}

//	std::cout << "Homography: R_norm = " << params.R_norm << ", overlap = " << params.overlap
//				<< ", H_xx = " << params.H_xy(0,0) << ", H_yy = " << params.H_xy(1,1)
//				<< ", H_xy = " << params.H_xy(0,1) << std::endl;

	if(debug) {
		glUseProgram(prog_debug);

		GL_bind_tex(prog_debug, "uImg", img->id, 0);
		GL_bind_tex(prog_debug, "uRes", tex_residual->id, 1);

		render::fullscreen(fbo_debug, width, height);

		GL_finish("Homography::solve()");
	}
	std::cout << "Homography[" << width << "x" << height << "]: took "
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
	auto fs_debug    = GL_compile_shader(GL_FRAGMENT_SHADER, "shader/debug/residual_overlay.glsl");

	const auto vs = render::get_fullscreen_vertex_shader();
	prog_jacobian = GL_link_program(vs, fs_jacobian);
	prog_gradient = GL_link_program(vs, fs_gradient);
	prog_debug  = GL_link_program(vs, fs_debug);

	glDeleteShader(fs_jacobian);
	glDeleteShader(fs_gradient);

	tex_uv       = std::make_shared<GL_Tex2D>(width, height, GL_RG16F, GL_RG, GL_HALF_FLOAT);
	tex_residual = std::make_shared<GL_Tex2D>(width, height, GL_RG16F, GL_RG, GL_HALF_FLOAT);

	for(int i = 0; i < 2; ++i) {
		tex_jacobian[i] = std::make_shared<GL_Tex2D>(width, height, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
		tex_gradient[i] = std::make_shared<GL_Tex2D>(width, reduction_chunk, GL_RGBA32F, GL_RGBA, GL_FLOAT);
	}
	for(int i = 0; i < 2; ++i) {
		tex_hessian.push_back(
				std::make_shared<GL_Tex2D>(width, reduction_chunk, GL_RGBA32F, GL_RGBA, GL_FLOAT));
	}
	tex_RwHxy = std::make_shared<GL_Tex2D>(width, reduction_chunk, GL_RGBA32F, GL_RGBA, GL_FLOAT);

	fbo_jacobian = GL_create_FBO(
			{tex_jacobian[0]->id, tex_jacobian[1]->id, tex_residual->id, tex_uv->id});
	fbo_gradient = GL_create_FBO(
			{tex_gradient[0]->id, tex_gradient[1]->id, tex_hessian[0]->id, tex_hessian[1]->id, tex_RwHxy->id});

	tex_debug = std::make_shared<GL_Tex2D>(width, height, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);
	fbo_debug = GL_create_FBO(tex_debug->id);

	have_init = true;
}

Homography::~Homography()
{
	glDeleteProgram(prog_jacobian);
	glDeleteProgram(prog_gradient);

	glDeleteFramebuffers(1, &fbo_jacobian);
	glDeleteFramebuffers(1, &fbo_gradient);
}


} // mmpilot
