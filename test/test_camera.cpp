/*
 * test_camera.cpp
 *
 *  Created on: Feb 8, 2026
 *      Author: mad
 */

#include <mmpilot/camera.h>

#include <string>
#include <iostream>

using namespace mmpilot;


uint64_t last_ts = 0;

void handle(const Camera::Frame& frame)
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
	last_ts = frame.timestamp;
}


int main(int argc, char** argv)
{
	int index			= argc > 1 ? atoi(argv[0]) : 0;
	int width 			= argc > 2 ? atoi(argv[1]) : 1280;
	int height 			= argc > 3 ? atoi(argv[2]) : 720;
	int fps				= argc > 4 ? atoi(argv[3]) : 10;
	std::string format	= argc > 5 ? argv[4] : "SRGGB8";

	std::cout << "index = " << index << std::endl;
	std::cout << "width = " << width << std::endl;
	std::cout << "height = " << height << std::endl;
	std::cout << "format = " << format << std::endl;

	Camera::init();

	Camera cam(index, 0, width, height, format);

	cam.open();

	cam.on_frame = &handle;

	int frame_us = 1000000 / fps;
	cam.controls().set(libcamera::controls::FrameDurationLimits,
			libcamera::Span<const int64_t, 2>({frame_us, 2 * frame_us}));

	cam.controls().set(libcamera::controls::ExposureTime, 5000);

	cam.start();

	std::string cmd;
	std::cin >> cmd;

	cam.stop();

	Camera::cleanup();

	return 0;
}

