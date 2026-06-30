/*
 * mmpilot_nav.cpp
 *
 *  Created on: Jun 29, 2026
 *      Author: mad
 */

// Generic Navigation Stack

#include <mmpilot/navigation.h>
#include <mmpilot/camera.h>
#include <mmpilot/image.h>

#include <iostream>


int main(int argc, char** argv)
{
	int camera_index = 0;
	int camera_stream = 0;
	int camera_interval_ms = 200;

	int sensor_width = 1640;
	int sensor_height = 1232;

	int msp_inverval_ms = 20;

	std::string msp_port = "/dev/ttyAMA0";


	Navigation nav;
	// TODO: settings

	nav.init(sensor_width, sensor_height);


	MSP2 msp(msp_port);
	msp.interval = std::chrono::milliseconds(msp_inverval_ms);

	nav.pipe.connect(msp);

	msp.start();


	Camera::init();

	auto cam = std::make_unique<Camera>(camera_index, camera_stream, sensor_width, sensor_height, "YUV420");

	cam->open();
	cam->set_interval(camera_interval_ms);

	cam->on_frame = [&](const CameraFrame& frame) {
		nav.pipe.handle(frame.convert());
		nav.pipe.sync();
	};
	cam->start();


	wait_for_exit();

	msp.shutdown();

	cam->stop();

	cam = nullptr;

	Camera::cleanup();

	return 0;
}

