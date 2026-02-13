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
	typedef std::array<float, 8> Params8;

	float damping = 0;
	int num_iters = 9;
	int reduction_chunk = 32;

	std::shared_ptr<GL_Tex2D> tex_uv;					// (u, v)
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

	GLuint fbo_jacobian = 0;
	GLuint fbo_gradient = 0;
	GLuint fbo_hessian = 0;

	bool have_init = false;

};


} // mmpilot

#endif /* INCLUDE_MMPILOT_HOMOGRAPHY_H_ */
