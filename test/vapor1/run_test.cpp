/*
 * run_test.cpp
 *
 *  Created on: Feb 12, 2026
 *      Author: mad
 */

#include <mmpilot/camera.h>
#include <mmpilot/sample.h>
#include <mmpilot/util.h>
#include <mmpilot/jpeg.h>
#include <mmpilot/image.h>

#include "pipeline.h"

#include <string>
#include <memory>
#include <iostream>

std::shared_ptr<Image> convert(const CameraFrame& frame)
{
	if(frame.data.size() != 3) {
		return nullptr;
	}
	const auto& Y = frame.data[0];
	const auto& U = frame.data[1];
	const auto& V = frame.data[2];

	auto out = std::make_shared<Image>();
	out->width = frame.width;
	out->height = frame.height;
	out->stride = frame.width;
	out->exposure = frame.exposure;
	out->analog_gain = frame.analog_gain;
	out->sequence = frame.sequence;
	out->timestamp = frame.timestamp / 1000;
	out->format = "YUV420";

	out->data.emplace_back((const uint8_t*)Y.first, (const uint8_t*)Y.first + Y.second);
	out->data.emplace_back((const uint8_t*)U.first, (const uint8_t*)U.first + U.second);
	out->data.emplace_back((const uint8_t*)V.first, (const uint8_t*)V.first + V.second);

	std::cout << "Frame " << frame.sequence << ": ts = " << frame.timestamp
			<< ", width = " << frame.width << ", height = " << frame.height
			<< ", stride = " << frame.stride << ", format = " << frame.pixel_format
			<< ", planes = " << out->data.size() << std::endl;
	return out;
};


int main(int argc, char** argv)
{
	Pipeline pipe_0;
	Pipeline pipe_1;

	Camera::init();

	auto cam_0 = std::make_unique<Camera>(0, 0, 2304, 1296, "YUV420");
	auto cam_1 = std::make_unique<Camera>(1, 0, 1640, 1232, "YUV420");

	cam_0->open();
	cam_1->open();

	cam_0->on_frame = [&](const CameraFrame& frame) {
		pipe_0.handle(convert(frame));
	};
	cam_1->on_frame = [&](const CameraFrame& frame) {
		pipe_1.handle(convert(frame));
	};

	cam_0->set_interval(500);
	cam_1->set_interval(500);

	cam_0->start();
	cam_1->start();

	wait_for_exit();

	cam_0->stop();
	cam_1->stop();

	cam_0 = nullptr;
	cam_1 = nullptr;

	Camera::cleanup();

	return 0;
}


