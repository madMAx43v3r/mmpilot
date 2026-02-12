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
#include <iostream>


int main(int argc, char** argv)
{
	const int quality = argc > 1 ? atoi(argv[1]) : 95;
	std::string file_name = argc > 2 ? argv[2] : "vapor1_record.dat";

	std::cout << "quality = " << quality << std::endl;
	std::cout << "file_name = " << file_name << std::endl;

	Thread gl_main(&gl_main_func);

	Pipeline pipe_0;
	Pipeline pipe_1;

	Camera::init();

	auto cam_0 = std::make_unique<Camera>(0, 0, 2304, 1296, "YUV420");
	auto cam_1 = std::make_unique<Camera>(1, 0, 1640, 1232, "YUV420");

	cam_0->open();
	cam_1->open();

	cam_0->on_frame = [&](const CameraFrame& frame) {
		gl_main.post(std::bind(&Pipeline::exec_frame, &pipe_0, frame));
	};
	cam_1->on_frame = [&](const CameraFrame& frame) {
		gl_main.post(std::bind(&Pipeline::exec_frame, &pipe_1, frame));
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

	rec.close();

	return 0;
}


