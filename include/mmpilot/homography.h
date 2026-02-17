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

		Params apply(float width, float height) const {
			if(is_applied) {
				throw std::logic_error("Params: already applied");
			}
			Params out(*this);
			out.p(6) /= width;
			out.p(7) /= height;
			out.is_applied = true;
			return out;
		}

		Mat3f matrix() const {
			if(!is_applied) {
				throw std::logic_error("Params: not applied");
			}
			Mat3f M;
			M << p(0), p(1), p(2),
				 p(3), p(4), p(5),
				 p(6), p(7), 1.0f;
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
			if(!is_applied) {
				throw std::logic_error("Params: not applied");
			}
			return Params(matrix().inverse());
		}

		Params applied_inverse(float width, float height) const {
			if(is_applied) {
				throw std::logic_error("Params: already applied");
			}
			return apply(width, height).inverse();
		}

		Transform2D transform() const;

		Vec2f project(const Vec2f& v, const Vec2f& center = Vec2f::Zero()) const {
			if(!is_applied) {
				throw std::logic_error("Params: not applied");
			}
			const Vec2f q = v - center;
			const float w = p(6) * q.x() + p(7) * q.y() + 1;
			return center + Vec2f(
				(p(0) * q.x() + p(1) * q.y() + p(2)) / w,
				(p(3) * q.x() + p(4) * q.y() + p(5)) / w
			);
		}

		Vec3f project3(const Vec2f& v, const Vec2f& center = Vec2f::Zero()) const {
			if(!is_applied) {
				throw std::logic_error("Params: not applied");
			}
			const Vec2f q = v - center;
			const float w = p(6) * q.x() + p(7) * q.y() + 1;
			return Vec3f(
				(p(0) * q.x() + p(1) * q.y() + p(2)) / w + center.x(),
				(p(3) * q.x() + p(4) * q.y() + p(5)) / w + center.y(),
				w
			);
		}
	private:
		bool is_applied = false;

		Params(const Mat3f& M)
		{
			p(0) = M(0,0);  p(1) = M(0,1);  p(2) = M(0,2);
			p(3) = M(1,0);  p(4) = M(1,1);  p(5) = M(1,2);
			p(6) = M(2,0);  p(7) = M(2,1);
			for(int i = 0; i < 8; ++i) {
				p(i) /= M(2,2);
			}
			is_applied = true;
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
