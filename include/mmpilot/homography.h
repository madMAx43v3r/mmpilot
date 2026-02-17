/*
 * homography.h
 *
 *  Created on: Feb 5, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_HOMOGRAPHY_H_
#define INCLUDE_MMPILOT_HOMOGRAPHY_H_

#include <mmpilot/texture.h>
#include <mmpilot/opengl.h>
#include <mmpilot/math.h>

#include <array>


namespace mmpilot {

class Homography {
public:
	float damping = 1;				// H diag factor
	float proj_damping = 10;		// extra damping for p6,p7

	int num_iters = 8;
	int reduction_chunk = 32;

	std::shared_ptr<GL_Tex2D> tex_uv;					// (u, v)
	std::shared_ptr<GL_Tex2D> tex_debug;				// (RGBA)
	std::shared_ptr<GL_Tex2D> tex_residual;				// (R, w)
	std::shared_ptr<GL_Tex2D> tex_gradient[2];
	std::shared_ptr<GL_Tex2D> tex_jacobian[2];
	std::vector<std::shared_ptr<GL_Tex2D>> tex_hessian;

	// [0 1 2]
	// [3 4 5]
	// [6 7 X]
	class Params : public std::array<float, 8> {
	public:
		Params() {
			for(int i = 0; i < 8; ++i) p(i) = 0;
			p(0) = 1;
			p(4) = 1;
		}

		float& p(size_t i) {
			return (*this)[i];
		}

		const float& p(size_t i) const {
			return (*this)[i];
		}

		void scale(float s) {
			p(2) *= s; p(5) *= s;
		}

		void shift(float x, float y) {
			p(2) += x; p(5) += y;
		}

		void normalize() {
			const auto s = 1 / Vec3f(p(6), p(7), 1).norm();
			for(int i = 0; i < 8; ++i) {
				p(i) *= s;
			}
		}

		Vec2f get_shift() const {
			return Vec2f(p(2), p(5));
		}
	};

	~Homography();

	Params solve(std::shared_ptr<GL_Tex2D> ref, std::shared_ptr<GL_Tex2D> img);

	Params solve(std::shared_ptr<GL_Tex2D> ref, std::shared_ptr<GL_Tex2D> img, const Params& init_p);

	void init(int width, int height);

private:
	int width = 0;
	int height = 0;

	GLuint prog_jacobian = 0;
	GLuint prog_gradient = 0;
	GLuint prog_debug = 0;

	GLuint fbo_jacobian = 0;
	GLuint fbo_gradient = 0;
	GLuint fbo_debug = 0;

	bool have_init = false;

};


} // mmpilot

#endif /* INCLUDE_MMPILOT_HOMOGRAPHY_H_ */
