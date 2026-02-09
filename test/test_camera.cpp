/*
 * test_camera.cpp
 *
 *  Created on: Feb 8, 2026
 *      Author: mad
 */

#include <mmpilot/camera.h>
#include <mmpilot/util.h>

#include <string>
#include <iostream>

using namespace mmpilot;


uint64_t last_ts = 0;
uint64_t last_seq = 0;

void handle(const CameraFrame& frame)
{
	std::cout << "Frame " << frame.sequence << ": ts = " << frame.timestamp
			<< ", width = " << frame.width << ", height = " << frame.height
			<< ", stride = " << frame.stride
			<< ", format = " << frame.pixel_format << std::endl;

	for(const auto& buf : frame.data) {
		std::cout << "  Buffer: size = " << buf.second << std::endl;
	}
	if(last_ts) {
		std::cout << "  Interval: " << (frame.timestamp - last_ts) / 1000 << " us" << std::endl;
	}
	if(frame.sequence > last_seq + 1) {
		std::cout << "  WARNING: Dropped " << (frame.sequence - last_seq - 1) << " frames !!!!!" << std::endl;
	}
	last_ts = frame.timestamp;
	last_seq = frame.sequence;
}


int main(int argc, char** argv)
{
	int index			= argc > 1 ? atoi(argv[1]) : 0;
	int width 			= argc > 2 ? atoi(argv[2]) : 1280;
	int height 			= argc > 3 ? atoi(argv[3]) : 720;
	int fps				= argc > 4 ? atoi(argv[4]) : 0;
	std::string format	= argc > 5 ? argv[5] : "SRGGB8";

	std::cout << "index = " << index << std::endl;
	std::cout << "width = " << width << std::endl;
	std::cout << "height = " << height << std::endl;
	std::cout << "format = " << format << std::endl;

	Camera::init();

	auto cam = std::make_unique<Camera>(index, 0, width, height, format);

	cam->open();

	cam->on_frame = &handle;

	if(fps > 0) {
		cam->set_interval(1000 / fps);
	}

	cam->start();

	wait_for_exit();

	cam->stop();
	cam = nullptr;

	Camera::cleanup();

	return 0;
}

