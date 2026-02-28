/*
 * affine.h
 *
 *  Created on: Feb 27, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_AFFINE_H_
#define INCLUDE_MMPILOT_AFFINE_H_

#include <mmpilot/texture.h>
#include <mmpilot/opengl.h>
#include <mmpilot/transform.h>

#include <array>


namespace mmpilot {

class Affine {
public:
	float damping = 1;				// H diag factor

	int num_iters = 8;
	int reduction_chunk = 32;

	bool debug = false;

	std::shared_ptr<GL_Tex2D> tex_debug;				// (RGBA)
	std::shared_ptr<GL_Tex2D> tex_residual;				// (R, w)
	std::shared_ptr<GL_Tex2D> tex_RwHxy;				// (R, w, H_xy)
	std::shared_ptr<GL_Tex2D> tex_gradient;
	std::shared_ptr<GL_Tex2D> tex_hessian;
	std::shared_ptr<GL_Tex2D> tex_jacobian;

	// (x, y, alpha, scale)
	class Params : public std::array<float, 4> {
	public:
		float R_norm = 0;		// normalized (factor 1000)
		float overlap = 0;		// (0 to 1)
		Mat2f H_xy = Mat2f::Identity();

		Params() {
			p(0) = p(1) = p(2) = 0;
			p(3) = 1;
		}

		Params(float x, float y, float a, float s) {
			p(0) = x;
			p(1) = y;
			p(2) = a;
			p(3) = s;
		}

		Params(const Params&) = default;

		float& p(size_t i) {
			return (*this)[i];
		}

		const float& p(size_t i) const {
			return (*this)[i];
		}

		float yaw() const {
			return p(2);
		}

		float scale() const {
			return p(3);
		}

		Params& scale(float s) {
			p(0) *= s;
			p(1) *= s;
			return *this;
		}

		Params& shift(float x, float y) {
			p(0) += x; p(1) += y;
			return *this;
		}

		Mat3f matrix() const {
			const Mat2f A = rotation() * p(3);
			Mat3f M;
			M << A(0,0), A(0,1), p(0),
				 A(1,0), A(1,1), p(1),
				 0.f,  0.f,  1.0f;
			return M;
		}

		Mat2f rotation() const {
			return get_rotation_matrix(p(2));
		}

		Vec2f translation() const {
			return Vec2f(p(0), p(1));
		}

		Params inverse() const {
			const Vec2f t = -(rotation().transpose() * translation()) / scale();
			return Params(t.x(), t.y(), -p(2), 1 / p(3));
		}

		Transform2D transform() const
		{
			Transform2D out;
			out.rot = rotation();
			out.pos = translation();
			out.scale = scale();
			return out;
		}

		Vec2f project(const Vec2f& v, const Vec2f& center = Vec2f::Zero()) const
		{
			const Vec2f p = v - center;
			const Vec3f q = matrix() * Vec3f(p.x(), p.y(), 1);
			return center + Vec2f(q.x(), q.y());
		}
	};

	~Affine();

	Params exec(std::shared_ptr<GL_Tex2D> ref, std::shared_ptr<GL_Tex2D> img, const Params& init_p);

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
	std::vector<float> G_buf, H_buf, RwHxy_buf;

	bool have_init = false;

};


} // mmpilot

#endif /* INCLUDE_MMPILOT_AFFINE_H_ */
