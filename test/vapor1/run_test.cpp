/*
 * run_test.cpp
 *
 *  Created on: Feb 12, 2026
 *      Author: mad
 */

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


	MSP2 msp(msp_port);
	msp.interval = std::chrono::milliseconds(msp_inverval_ms);

	Navigation nav(&msp);
	nav.pipe.do_record = true;
	nav.pipe.src_flip_y = true;
	nav.pipe.radius_mask = 0.9;
	nav.virtual_cam.FOV_in = 190;
	nav.virtual_cam.FOV_cam = 120;
	nav.virtual_cam.RPY_cam = Vec3f(0, 0, -30 -90);
	nav.virtual_cam.cam_model = 3;
	nav.virtual_cam.K_param  = Vec2f(-0.01, -0.01);		// stereo

	nav.init(sensor_width, sensor_height);

	nav.pipe.connect(&msp);

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


