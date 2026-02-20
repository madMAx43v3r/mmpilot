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
#include <mmpilot/transform.h>

#include <array>


namespace mmpilot {

class Homography {
public:
	float damping = 1;				// H diag factor

	int num_iters = 8;
	int reduction_chunk = 32;

	bool debug = true;

	std::shared_ptr<GL_Tex2D> tex_uv;					// (u, v)
	std::shared_ptr<GL_Tex2D> tex_debug;				// (RGBA)
	std::shared_ptr<GL_Tex2D> tex_residual;				// (R, w)
	std::shared_ptr<GL_Tex2D> tex_RwHxy;				// (R, w, H_xy)
	std::shared_ptr<GL_Tex2D> tex_gradient[3];			// 6x G + 6x H
	std::shared_ptr<GL_Tex2D> tex_jacobian[2];

	// [0 1 2]
	// [3 4 5]
	// [X X X]
	class Params : public std::array<float, 6> {
	public:
		float R_norm = 0;		// normalized (factor 100)
		float overlap = 0;		// (0 to 1)
		Mat2f H_xy = Mat2f::Identity();

		Params() {
			for(int i = 0; i < 6; ++i) p(i) = 0;
			p(0) = 1;
			p(4) = 1;
		}

		Params(const Params&) = default;

		float& p(size_t i) {
			return (*this)[i];
		}

		const float& p(size_t i) const {
			return (*this)[i];
		}

		Params& scale(float s) {
			p(2) *= s; p(5) *= s;
			return *this;
		}

		Params& shift(float x, float y) {
			p(2) += x; p(5) += y;
			return *this;
		}

		Mat3f matrix() const {
			Mat3f M;
			M << p(0), p(1), p(2),
				 p(3), p(4), p(5),
				 0.f,  0.f,  1.0f;
			return M;
		}

		Mat2f rotation() const {
			Mat2f M;
			M << p(0), p(1),
				 p(3), p(4);
			return M;
		}

		Vec2f translation() const {
			return Vec2f(p(2), p(5));
		}

		Params inverse() const {
			return Params(matrix().inverse());
		}

		Transform2D transform() const;

		Vec2f project(const Vec2f& v, const Vec2f& center = Vec2f::Zero()) const
		{
			const Vec2f q = v - center;
			return center + Vec2f(
				(p(0) * q.x() + p(1) * q.y() + p(2)),
				(p(3) * q.x() + p(4) * q.y() + p(5))
			);
		}
	private:
		Params(const Mat3f& M) {
			p(0) = M(0,0);  p(1) = M(0,1);  p(2) = M(0,2);
			p(3) = M(1,0);  p(4) = M(1,1);  p(5) = M(1,2);
			for(int i = 0; i < 6; ++i) {
				p(i) /= M(2,2);
			}
		}
	};

	~Homography();

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

	// CPU readback buffers
	std::vector<float> G0_buf, D0_buf, GD_buf, RwHxy_buf;

	bool have_init = false;

};


} // mmpilot

#endif /* INCLUDE_MMPILOT_HOMOGRAPHY_H_ */
