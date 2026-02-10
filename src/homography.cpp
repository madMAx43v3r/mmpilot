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

#include <mmpilot/opengl.h>
#include <mmpilot/render.h>
#include <mmpilot/homography.h>

#include <GLES3/gl31.h>
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

using Mat8 = Eigen::Matrix<float, 8, 8, Eigen::RowMajor>;
using Vec8 = Eigen::Matrix<float, 8, 1>;
using Params8 = std::array<float, 8>;

// ------------------------ Solver Assembly ------------------------

struct NormalEquations {
	Vec8 g;
	Mat8 H;
	NormalEquations() {
		g.setZero();
		H.setZero();
	}
};

static NormalEquations assemble_equations(
		const std::vector<float>& G0_rgba, // b0..b3
		const std::vector<float>& G1_rgba, // b4..b7
		const std::vector<float>& D0_rgba, // d0..d3
		const std::vector<float>& D1_rgba, // d4..d7
		const std::array<std::vector<float>, 7>& S_rgba)
{
	NormalEquations ne;

	auto sumRGBA = [&](const std::vector<float>& v, float out[4]) {
		out[0] = out[1] = out[2] = out[3] = 0;
		const auto N = v.size() / 4;
		for(size_t i = 0; i < N; ++i) {
			out[0] += v[i * 4 + 0];
			out[1] += v[i * 4 + 1];
			out[2] += v[i * 4 + 2];
			out[3] += v[i * 4 + 3];
		}
	};

	float b0_3[4], b4_7[4], d0_3[4], d4_7[4];
	sumRGBA(G0_rgba, b0_3);
	sumRGBA(G1_rgba, b4_7);
	sumRGBA(D0_rgba, d0_3);
	sumRGBA(D1_rgba, d4_7);

	// Gradient g = sum(J^T r). Your shader accumulates G += J * R.
	// Normal equations: H * delta = -g.
	for(int i = 0; i < 4; ++i) {
		ne.g(i)		= b0_3[i];
		ne.g(i + 4)	= b4_7[i];
	}

	// Diagonal of H: sum(Jk^2)
	for(int i = 0; i < 4; ++i) {
		ne.H(i, i)			= d0_3[i];
		ne.H(i + 4, i + 4)	= d4_7[i];
	}

	// Off-diagonals mapping from your MRT packing:
	// S0: (0,1),(0,2),(0,3),(0,4)
	// S1: (0,5),(0,6),(0,7),(1,2)
	// S2: (1,3),(1,4),(1,5),(1,6)
	// S3: (1,7),(2,3),(2,4),(2,5)
	// S4: (2,6),(2,7),(3,4),(3,5)
	// S5: (3,6),(3,7),(4,5),(4,6)
	// S6: (4,7),(5,6),(5,7),(6,7)
	float s[7][4];
	for(int i = 0; i < 7; ++i) {
		sumRGBA(S_rgba[i], s[i]);
	}

	auto setH = [&](int i, int j, float v) {
		ne.H(i, j) = v;
		ne.H(j, i) = v;
	};

	setH(0, 1, s[0][0]);
	setH(0, 2, s[0][1]);
	setH(0, 3, s[0][2]);
	setH(0, 4, s[0][3]);
	setH(0, 5, s[1][0]);
	setH(0, 6, s[1][1]);
	setH(0, 7, s[1][2]);
	setH(1, 2, s[1][3]);
	setH(1, 3, s[2][0]);
	setH(1, 4, s[2][1]);
	setH(1, 5, s[2][2]);
	setH(1, 6, s[2][3]);
	setH(1, 7, s[3][0]);
	setH(2, 3, s[3][1]);
	setH(2, 4, s[3][2]);
	setH(2, 5, s[3][3]);
	setH(2, 6, s[4][0]);
	setH(2, 7, s[4][1]);
	setH(3, 4, s[4][2]);
	setH(3, 5, s[4][3]);
	setH(3, 6, s[5][0]);
	setH(3, 7, s[5][1]);
	setH(4, 5, s[5][2]);
	setH(4, 6, s[5][3]);
	setH(4, 7, s[6][0]);
	setH(5, 6, s[6][1]);
	setH(5, 7, s[6][2]);
	setH(6, 7, s[6][3]);
	return ne;
}

void apply_damping(Mat8& H, float lambda)
{
	for(int i = 0; i < 8; ++i) {
		H(i, i) += lambda;
	}
}

// ------------------------ Main entry for one solve ------------------------

struct GNConfig {
	int num_iters = 10;
	int chunk_size = 32;	// output height for reduction passes
	float lambda = 0;		// damping
};

// Provide your fragment shader sources (as C strings) for:
//  - fsJ: your first kernel (J + R + UV)
//  - fsRedGD: your second kernel (G + H diag)
//  - fsRedOff: your third kernel (off-diagonals)

Params8 solve_homography(
		int W, int H, GLuint texRef_R16F, GLuint texGrad_RGBA16F, const char* fsJ,
		const char* fsRedGD, const char* fsRedOff, Params8 p0, const GNConfig& cfg)
{
	// ---- Programs
	GLuint vs = GL_compile_shader(GL_VERTEX_SHADER, kFullscreenVS);
	GLuint fs1 = GL_compile_shader(GL_FRAGMENT_SHADER, fsJ);
	GLuint fs2 = GL_compile_shader(GL_FRAGMENT_SHADER, fsRedGD);
	GLuint fs3 = GL_compile_shader(GL_FRAGMENT_SHADER, fsRedOff);

	GLuint progJ = GL_link_program(vs, fs1);
	GLuint progRedGD = GL_link_program(vs, fs2);
	GLuint progRedOff = GL_link_program(vs, fs3);

	// ---- First pass MRT textures (W x H)
	GLuint texJ0 = GL_create_tex(W, H, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
	GLuint texJ1 = GL_create_tex(W, H, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
	GLuint texRw = GL_create_tex(W, H, GL_RG16F, GL_RG, GL_HALF_FLOAT);
	GLuint texUV = GL_create_tex(W, H, GL_RG16F, GL_RG, GL_HALF_FLOAT);

	GLuint fboJ = GL_create_FBO( {texJ0, texJ1, texRw, texUV});

	// ---- Reduction pass outputs (W x chunkSize)
	const int rW = W;
	const int rH = cfg.chunk_size;

	GLuint texG0 = GL_create_tex(rW, rH, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
	GLuint texG1 = GL_create_tex(rW, rH, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
	GLuint texD0 = GL_create_tex(rW, rH, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
	GLuint texD1 = GL_create_tex(rW, rH, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);

	GLuint fboGD = GL_create_FBO( {texG0, texG1, texD0, texD1});

	// Off-diagonal textures (7 MRTs)
	GLuint texS0 = GL_create_tex(rW, rH, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
	GLuint texS1 = GL_create_tex(rW, rH, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
	GLuint texS2 = GL_create_tex(rW, rH, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
	GLuint texS3 = GL_create_tex(rW, rH, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
	GLuint texS4 = GL_create_tex(rW, rH, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
	GLuint texS5 = GL_create_tex(rW, rH, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);
	GLuint texS6 = GL_create_tex(rW, rH, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);

	GLuint fboOff = GL_create_FBO( {texS0, texS1, texS2, texS3, texS4, texS5, texS6});

	// ---- A dummy VAO is required in core-ish profiles; ES typically too.
	GLuint vao = 0;
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	Params8 p = p0;

	// CPU readback buffers
	std::vector<float> G0_rgba, G1_rgba, D0_rgba, D1_rgba;
	std::array<std::vector<float>, 7> S_rgba;

	for(int iter = 0; iter < cfg.num_iters; ++iter)
	{
		// ---------- (1) Per-pixel J + residual
		glBindFramebuffer(GL_FRAMEBUFFER, fboJ);
		glViewport(0, 0, W, H);
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_BLEND);

		glUseProgram(progJ);
		GL_bind_tex(progJ, "uRef", texRef_R16F, 0);
		GL_bind_tex(progJ, "uGrad", texGrad_RGBA16F, 1);
		GL_set_uniform_2f(progJ, "uInvSize", 1.0f / float(W), 1.0f / float(H));
		GL_set_uniform_array(progJ, "uParams", p);

		render::fullscreen();

		// ---------- (2) Reduce over Y chunks: G + diag(D)
		glBindFramebuffer(GL_FRAMEBUFFER, fboGD);
		glViewport(0, 0, rW, rH);

		glUseProgram(progRedGD);
		GL_bind_tex(progRedGD, "uRes", texRw, 0);
		GL_bind_tex(progRedGD, "uJ0", texJ0, 1);
		GL_bind_tex(progRedGD, "uJ1", texJ1, 2);
		GL_set_uniform_1i(progRedGD, "uHeight", H);
		GL_set_uniform_1i(progRedGD, "uChunkSize", cfg.chunk_size);

		render::fullscreen();

		// ---------- (3) Reduce off-diagonals
		glBindFramebuffer(GL_FRAMEBUFFER, fboOff);
		glViewport(0, 0, rW, rH);

		glUseProgram(progRedOff);
		GL_bind_tex(progRedOff, "uJ0", texJ0, 0);
		GL_bind_tex(progRedOff, "uJ1", texJ1, 1);
		GL_set_uniform_1i(progRedOff, "uHeight", H);
		GL_set_uniform_1i(progRedOff, "uChunkSize", cfg.chunk_size);

		render::fullscreen();

		// Ensure rendering finished before readback (simple path).
		GL_finish("Homography::solve()");

		// ---------- Read back partials (W * chunkSize pixels)
		GL_read_FBO_RGBA(fboGD, 0, rW, rH, G0_rgba);
		GL_read_FBO_RGBA(fboGD, 1, rW, rH, G1_rgba);
		GL_read_FBO_RGBA(fboGD, 2, rW, rH, D0_rgba);
		GL_read_FBO_RGBA(fboGD, 3, rW, rH, D1_rgba);

		GL_read_FBO_RGBA(fboOff, 0, rW, rH, S_rgba[0]);
		GL_read_FBO_RGBA(fboOff, 1, rW, rH, S_rgba[1]);
		GL_read_FBO_RGBA(fboOff, 2, rW, rH, S_rgba[2]);
		GL_read_FBO_RGBA(fboOff, 3, rW, rH, S_rgba[3]);
		GL_read_FBO_RGBA(fboOff, 4, rW, rH, S_rgba[4]);
		GL_read_FBO_RGBA(fboOff, 5, rW, rH, S_rgba[5]);
		GL_read_FBO_RGBA(fboOff, 6, rW, rH, S_rgba[6]);

		// ---------- Assemble and solve
		NormalEquations ne = assemble_equations(rW, rH, G0_rgba, G1_rgba, D0_rgba, D1_rgba, S_rgba);

		Mat8 Hmat = ne.H;
		Vec8 gvec = ne.g;

		apply_damping(Hmat, cfg.lambda);

		// Solve H * delta = -g
		Eigen::LDLT<Mat8> ldlt(Hmat);
		if(ldlt.info() != Eigen::Success) {
			throw std::logic_error("LDLT failed");
		}
		const Vec8 delta = ldlt.solve(-gvec);

		// Update params
		const auto step_norm = delta.norm();

		std::cerr << "iter " << iter << ": delta = " << step_norm << std::endl;

		if(!std::isfinite(step_norm)) {
			throw std::logic_error("solver failed");
		}
		for(int i = 0; i < 8; ++i) {
			p[i] += delta[i];
		}

		// renormalize homography scale to keep numbers sane
		// Here, keep p2 / p5 roughly on the same scale by normalizing by (p6,p7,1) magnitude.
		const auto s = std::sqrt(p[6]*p[6] + p[7]*p[7] + 1);
		for(float& v : p) {
			v /= s;
		}
	}

	// Cleanup (trim as you like)
	glDeleteFramebuffers(1, &fboJ);
	glDeleteFramebuffers(1, &fboGD);
	glDeleteFramebuffers(1, &fboOff);

	GLuint texs[] = {texJ0, texJ1, texRw, texUV, texG0, texG1, texD0, texD1, texS0, texS1, texS2, texS3, texS4, texS5,
			texS6};
	glDeleteTextures((GLsizei)(sizeof(texs) / sizeof(texs[0])), texs);

	glDeleteProgram(progJ);
	glDeleteProgram(progRedGD);
	glDeleteProgram(progRedOff);

	glDeleteShader(vs);
	glDeleteShader(fs1);
	glDeleteShader(fs2);
	glDeleteShader(fs3);

	glDeleteVertexArrays(1, &vao);

	return p;
}

void Homography::init()
{
	fs_jacobian = GL_compile_shader(GL_FRAGMENT_SHADER, "shader/homographic/jacobian.glsl");
	fs_gradient = GL_compile_shader(GL_FRAGMENT_SHADER, "shader/homographic/gradient.glsl");
	fs_hessian  = GL_compile_shader(GL_FRAGMENT_SHADER, "shader/homographic/hessian.glsl");

	const auto vs = render::get_fullscreen_vertex_shader();
	prog_jacobian = GL_link_program(vs, fs_jacobian);
	prog_gradient = GL_link_program(vs, fs_gradient);
	prog_hessian  = GL_link_program(vs, fs_hessian);
}

void Homography::cleanup()
{
	glDeleteProgram(prog_jacobian);
	glDeleteProgram(prog_gradient);
	glDeleteProgram(prog_hessian);

	glDeleteShader(fs_jacobian);
	glDeleteShader(fs_gradient);
	glDeleteShader(fs_hessian);
}


} // mmpilot

// ------------------------ Example usage ------------------------
//
// You’d call solveHomographyGN(...) from your app after you’ve created
// texRef_R16F and texGrad_RGBA16F and loaded your fragment sources.
//
/*
 int main() {
 // 1) Create EGL context + make current
 // 2) Upload textures texRef_R16F, texGrad_RGBA16F
 // 3) Provide fragment shader strings fsJ, fsRedGD, fsRedOff

 Params8 p0 = {1,0,0, 0,1,0, 0,0}; // identity in your 8-param form
 GNConfig cfg;
 cfg.maxIters = 20;
 cfg.chunkSize = 32;
 cfg.epsStep = 1e-6f;
 cfg.lambda = 1e-3f;

 Params8 p = solveHomographyGN(W,H, texRef, texGrad, fsJ, fsRedGD, fsRedOff, p0, cfg);

 // p defines:
 // u = (p0 x + p1 y + p2) / (p6 x + p7 y + 1)
 // v = (p3 x + p4 y + p5) / (p6 x + p7 y + 1)
 }
 */

