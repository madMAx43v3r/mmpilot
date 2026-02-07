/*
 * reduction.h
 *
 *  Created on: Feb 5, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_REDUCTION_H_
#define INCLUDE_MMPILOT_REDUCTION_H_

#include <GLES3/gl31.h>


namespace mmpilot {

class SumY {
public:
	SumY(int width, int height);

	void exec();

	static void init();

	static void cleanup();

private:
	static GLuint shader;
	static GLuint prog;

};


} // mmpilot

#endif /* INCLUDE_MMPILOT_REDUCTION_H_ */
