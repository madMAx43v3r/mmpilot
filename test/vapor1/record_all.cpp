/*
 * record_all.cpp
 *
 *  Created on: Feb 8, 2026
 *      Author: mad
 */

#include <mmpilot/camera.h>
#include <mmpilot/sample.h>
#include <mmpilot/util.h>

#include <string>
#include <iostream>
#include <mutex>

using namespace mmpilot;


std::mutex mutex;
Recorder* rec = nullptr;

void on_frame(const std::string& topic, const Camera::Frame& frame)
{
	{
		std::lock_guard<std::mutex> lock(mutex);
		std::cout << "Frame " << frame.sequence << ": ts = " << frame.timestamp
				<< ", width = " << frame.width << ", height = " << frame.height
				<< ", stride = " << frame.stride
				<< ", format = " << frame.pixel_format << std::endl;
	}
	write_sample(*rec, topic, frame);
}


int main(int argc, char** argv)
{
	std::string file_name = argc > 1 ? argv[1] : "vapor1_record.dat";

	rec = new Recorder(file_name);

	Camera::init();

	auto cam_0 = std::make_unique<Camera>(0, 0, 2304, 1296, "SBGGR16");
	auto cam_1 = std::make_unique<Camera>(1, 0, 1640, 1232, "SBGGR16");

	cam_0->open();
	cam_1->open();

	cam_0->on_frame = std::bind(on_frame, "camera.front", std::placeholders::_1);
	cam_1->on_frame = std::bind(on_frame, "camera.below", std::placeholders::_1);

	cam_0->set_interval(500);
	cam_1->set_interval(500);

	cam_0->start();
	cam_1->start();

	wait_for_exit();

	cam_0->stop();
	cam_1->stop();

	cam_0 = nullptr;
	cam_1 = nullptr;

	rec->close();
	delete rec;

	Camera::cleanup();

	return 0;
}


