/*
 * record_all.cpp
 *
 *  Created on: Feb 8, 2026
 *      Author: mad
 */

#include <mmpilot/camera.h>
#include <mmpilot/sample.h>
#include <mmpilot/util.h>
#include <mmpilot/jpeg.h>
#include <mmpilot/image.h>

#include <string>
#include <iostream>
#include <mutex>

using namespace mmpilot;


int main(int argc, char** argv)
{
	const int quality = argc > 1 ? atoi(argv[1]) : 95;
	std::string file_name = argc > 2 ? argv[2] : "bench_record.dat";

	std::cout << "quality = " << quality << std::endl;
	std::cout << "file_name = " << file_name << std::endl;

	std::mutex mutex;

	Recorder rec(file_name);

	auto on_frame = [&](const std::string& topic, const CameraFrame& frame)
	{
		std::lock_guard<std::mutex> lock(mutex);

		if(frame.data.size() != 3) {
			return;
		}
		const auto* Y = frame.data[0].first;
		const auto* U = frame.data[1].first;
		const auto* V = frame.data[2].first;

		Image out;
		out.width = frame.width;
		out.height = frame.height;
		out.stride = frame.width;
		out.exposure = frame.exposure;
		out.analog_gain = frame.analog_gain;
		out.sequence = frame.sequence;
		out.timestamp = frame.timestamp / 1000;
		out.format = "JPEG";
		out.data.push_back(encode_jpeg_i420(
				Y, U, V, frame.width, frame.height, frame.stride, quality));

		std::cout << "Frame " << frame.sequence << ": ts = " << frame.timestamp
				<< ", width = " << frame.width << ", height = " << frame.height
				<< ", stride = " << frame.stride << ", format = " << frame.pixel_format
				<< ", size = " << out.data.size() << std::endl;

		write_sample(rec, topic, out);
	};

	Camera::init();

	auto cam_0 = std::make_unique<Camera>(0, 0, 2304, 1296, "YUV420");
//	auto cam_1 = std::make_unique<Camera>(1, 0, 1640, 1232, "YUV420");

	cam_0->open();
//	cam_1->open();

	cam_0->on_frame = std::bind(on_frame, "camera.narrow", std::placeholders::_1);
//	cam_1->on_frame = std::bind(on_frame, "camera.wide", std::placeholders::_1);

	cam_0->set_interval(500);
//	cam_1->set_interval(500);

	cam_0->start();
//	cam_1->start();

	wait_for_exit();

	cam_0->stop();
//	cam_1->stop();

	cam_0 = nullptr;
//	cam_1 = nullptr;

	Camera::cleanup();

	rec.close();

	return 0;
}


