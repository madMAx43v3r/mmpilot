/*
 * run_test.cpp
 *
 *  Created on: Feb 12, 2026
 *      Author: mad
 */

#include <mmpilot/camera.h>
#include <mmpilot/image.h>
#include <mmpilot/util.h>
#include <mmpilot/beta_msp.h>

#include "../pipeline.h"

#include <string>
#include <memory>
#include <iostream>

std::shared_ptr<Image> convert(const CameraFrame& frame)
{
	if(frame.pixel_format != "YUV420") {
		throw std::logic_error("invalid pixel format");
	}
	const auto& Y = frame.data[0];
	const auto& U = frame.data[1];
	const auto& V = frame.data[2];

	auto out = std::make_shared<Image>();
	out->width = frame.width;
	out->height = frame.height;
	out->stride = frame.stride;
	out->exposure = frame.exposure;
	out->analog_gain = frame.analog_gain;
	out->sequence = frame.sequence;
	out->timestamp = frame.timestamp / 1000;
	out->format = frame.pixel_format;

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
	pipe_0.src_flip_y = true;
	pipe_0.radius_mask = 0.9;

	MSP2Client msp("/dev/ttyACM0");

	msp.on_raw_imu = [&](const MSP2Client::RawImu& imu) {
		pipe_0.handle(std::make_shared<MSP2Client::RawImu>(imu));
	};

	msp.on_attitude = [&](const MSP2Client::Attitude& att) {
		pipe_0.handle(std::make_shared<MSP2Client::Attitude>(att));
	};

	msp.on_rc = [&](const MSP2Client::RcPacket& rc) {
		pipe_0.handle(std::make_shared<MSP2Client::RcPacket>(rc));
	};

	msp.on_gps = [&](const MSP2Client::RawGPS& gps) {
		pipe_0.handle(std::make_shared<MSP2Client::RawGPS>(gps));
	};

	Camera::init();

	auto cam_0 = std::make_unique<Camera>(0, 0, 1640, 1232, "YUV420");

	cam_0->open();

	cam_0->on_frame = [&](const CameraFrame& frame) {
		pipe_0.handle(convert(frame));
		pipe_0.sync();
	};

	cam_0->set_interval(200);

	cam_0->start();

	std::thread msp_thread([&]() {
		msp.run();
	});

	wait_for_exit();

	msp.shutdown();
	msp_thread.join();

	cam_0->stop();

	cam_0 = nullptr;

	Camera::cleanup();

	return 0;
}


