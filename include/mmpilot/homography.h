/*
 * homography.h
 *
 *  Created on: Feb 5, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_HOMOGRAPHY_H_
#define INCLUDE_MMPILOT_HOMOGRAPHY_H_

#include <GLES3/gl31.h>


namespace mmpilot {

class Homography {
public:
	Homography(int width, int height);

	void solve();

	static void init();

	static void cleanup();

private:
	static GLuint fs_jacobian;
	static GLuint fs_gradient;
	static GLuint fs_hessian;

	static GLuint prog_jacobian;
	static GLuint prog_gradient;
	static GLuint prog_hessian;

};


} // mmpilot

#endif /* INCLUDE_MMPILOT_HOMOGRAPHY_H_ */
