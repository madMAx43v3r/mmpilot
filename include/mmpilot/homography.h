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

#include <array>


namespace mmpilot {

class Homography {
public:
	// [0 1 2]
	// [3 4 5]
	// [6 7 X]
	class Params8 : public std::array<float, 8> {
	public:
		Params8() {
			for(int i = 0; i < 8; ++i) (*this)[i] = 0;
			(*this)[0] = 1;
			(*this)[4] = 1;
		}
		void scale(float s) {
			(*this)[2] *= s;
			(*this)[5] *= s;
		}
		void shift(float x, float y) {
			(*this)[2] += x;
			(*this)[5] += y;
		}
	};

	float damping = 1;				// H diag factor
	int num_iters = 8;
	int reduction_chunk = 32;

	std::shared_ptr<GL_Tex2D> tex_uv;					// (u, v)
	std::shared_ptr<GL_Tex2D> tex_debug;				// (RGBA)
	std::shared_ptr<GL_Tex2D> tex_residual;				// (R, w)
	std::shared_ptr<GL_Tex2D> tex_gradient[2];
	std::shared_ptr<GL_Tex2D> tex_jacobian[2];
	std::vector<std::shared_ptr<GL_Tex2D>> tex_hessian;

	Homography() = default;

	~Homography();

	Params8 solve(std::shared_ptr<GL_Tex2D> ref, std::shared_ptr<GL_Tex2D> img);

	Params8 solve(std::shared_ptr<GL_Tex2D> ref, std::shared_ptr<GL_Tex2D> img, const Params8& init_p);

	void init(int width, int height);

private:
	int width = 0;
	int height = 0;

	GLuint prog_jacobian = 0;
	GLuint prog_gradient = 0;
	GLuint prog_hessian = 0;
	GLuint prog_debug = 0;

	GLuint fbo_jacobian = 0;
	GLuint fbo_gradient = 0;
	GLuint fbo_hessian = 0;
	GLuint fbo_debug = 0;

	bool have_init = false;

};


} // mmpilot

#endif /* INCLUDE_MMPILOT_HOMOGRAPHY_H_ */
