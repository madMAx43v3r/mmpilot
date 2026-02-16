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
#include <mmpilot/beta_msp.h>

#include <string>
#include <iostream>
#include <mutex>

using namespace mmpilot;


int main(int argc, char** argv)
{
	const int quality = argc > 1 ? atoi(argv[1]) : 95;
	std::string file_name = argc > 2 ? argv[2] : "vapor1_record.dat";

	std::cout << "quality = " << quality << std::endl;
	std::cout << "file_name = " << file_name << std::endl;

	std::mutex mutex;

	Recorder rec(file_name);

	MSP2Client msp("/dev/ttyACM0");

	msp.on_raw_imu = [&](const MSP2Client::RawImu& imu)
	{
		std::lock_guard<std::mutex> lock(mutex);
		write_sample(rec, "msp.raw_imu", imu);
	};

	msp.on_attitude = [&](const MSP2Client::Attitude& att)
	{
		std::lock_guard<std::mutex> lock(mutex);
		write_sample(rec, "msp.attitude", att);
	};

	msp.on_rc = [&](const MSP2Client::RcPacket& rc)
	{
		std::lock_guard<std::mutex> lock(mutex);
		write_sample(rec, "msp.rc", rc);
	};

	msp.on_gps = [&](const MSP2Client::RawGPS& gps)
	{
		std::lock_guard<std::mutex> lock(mutex);
		write_sample(rec, "msp.raw_gps", gps);
	};

	auto on_frame = [&](const std::string& topic, const CameraFrame& frame)
	{
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

		std::lock_guard<std::mutex> lock(mutex);

		std::cout << "Frame " << frame.sequence << ": ts = " << frame.timestamp
				<< ", width = " << frame.width << ", height = " << frame.height
				<< ", stride = " << frame.stride << ", format = " << frame.pixel_format
				<< ", size = " << out.data.size() << std::endl;

		write_sample(rec, topic, out);
	};

	Camera::init();

	auto cam_0 = std::make_unique<Camera>(0, 0, 1640, 1232, "YUV420");

	cam_0->open();
	cam_0->on_frame = std::bind(on_frame, "camera.wide", std::placeholders::_1);

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

	rec.close();

	return 0;
}


