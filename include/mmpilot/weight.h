/*
 * weight.h
 *
 *  Created on: Feb 11, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_WEIGHT_H_
#define INCLUDE_MMPILOT_WEIGHT_H_

#include <mmpilot/texture.h>
#include <mmpilot/opengl.h>

#include <memory>
#include <vector>


namespace mmpilot {

class WeightRadius {
public:
	WeightRadius(float radius) {
		radius_sq = radius * radius;
	}

	void init(int width, int height)
	{
		out = std::make_shared<GL_Tex2D>(width, height, GL_RG8, GL_RG, GL_UNSIGNED_BYTE);

		// TODO

		have_init = true;
	}

	void handle(std::shared_ptr<GL_Tex2D> in)
	{
		// TODO
	}

private:
	float radius_sq = 0;

	bool have_init = false;

	std::shared_ptr<GL_Tex2D> out;

};


} // mmpilot

#endif /* INCLUDE_MMPILOT_WEIGHT_H_ */
