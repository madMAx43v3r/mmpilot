/*
 * bayer.cpp
 *
 *  Created on: Feb 10, 2026
 *      Author: mad
 */

#include <mmpilot/bayer.h>


namespace mmpilot {

void DeBayer::init(int width_, int height_, std::string format_)
{
	width = width_;
	height = height_;
	format = format_;

	if(format == "SBGGR16") {
		input = std::make_shared<GL_Tex2D>(width, height, GL_R16UI, GL_RED, GL_UNSIGNED_SHORT);
	} else {
		throw std::runtime_error("DeBayer: invalid format: " + format);
	}
	have_init = true;
}

void DeBayer::handle(std::shared_ptr<CameraFrame> frame)
{
	if(have_init) {
		if(frame->width != width || frame->height != height || frame->pixel_format != format) {
			throw std::runtime_error("DeBayer: frame dimensions / format mismatch");
		}
	} else {
		init(frame->width, frame->height, frame->pixel_format);
	}

	if(frame->data.size() != 1) {
		throw std::runtime_error("DeBayer: invalid frame data");
	}
	input->upload(frame->data[0].first);

	// TODO

	GL_check("DeBayer::handle()");
}


} // mmpilot
