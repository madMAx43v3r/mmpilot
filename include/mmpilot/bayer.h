/*
 * bayer.h
 *
 *  Created on: Feb 10, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_BAYER_H_
#define INCLUDE_MMPILOT_BAYER_H_

#include <mmpilot/texture.h>
#include <mmpilot/camera_frame.h>

#include <memory>
#include <functional>


namespace mmpilot {

class DeBayer {
public:
	float gamma = 0.7;		// for RGBA8

	std::function<void(std::shared_ptr<GL_Tex2D>)> on_luma;		// RG16F (luma, 1)
	std::function<void(std::shared_ptr<GL_Tex2D>)> on_rgba;		// RGBA8 (RGB, 1)

	void init(int width, int height, std::string format);

	void handle(std::shared_ptr<CameraFrame> frame);

private:
	int width = 0;
	int height = 0;
	std::string format;

	std::shared_ptr<GL_Tex2D> input;
	std::shared_ptr<GL_Tex2D> out_luma;
	std::shared_ptr<GL_Tex2D> out_rgba;

	GLuint fs_luma = 0;
	GLuint fs_rgba = 0;
	GLuint prog_luma = 0;
	GLuint prog_rgba = 0;

	bool have_init = false;

};


} // mmpilot

#endif /* INCLUDE_MMPILOT_BAYER_H_ */
