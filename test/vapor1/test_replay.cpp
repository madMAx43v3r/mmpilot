/*
 * test_replay.cpp
 *
 *  Created on: Feb 9, 2026
 *      Author: mad
 */

#include <mmpilot/replay.h>
#include <mmpilot/image.h>

#include "../pipeline.h"

#include <iostream>


int main(int argc, char** argv)
{
	const int64_t offset_sec = argc > 1 ? atoi(argv[1]) : 0;
	const std::string file_name = argc > 2 ? argv[2] : "vapor1_record.dat";

	std::cout << "offset = " << offset_sec << " sec" << std::endl;
	std::cout << "file_name = " << file_name << std::endl;

	Pipeline pipe_0;
	pipe_0.src_flip_y = true;
	pipe_0.radius_mask = 0.9;

	const auto on_frame = [&](std::shared_ptr<Image> frame) {
		std::cout << "[" << frame->topic << "] ts = " << frame->timestamp
				<< ", width = " << frame->width << ", height = " << frame->height
				<< ", size = " << frame->data.size()
				<< ", exposure = " << frame->exposure << ", gain = " << frame->analog_gain << std::endl;
	};

	const auto on_frame_0 = [&](std::shared_ptr<Image> frame) {
		on_frame(frame);
		pipe_0.handle(frame);
		pipe_0.sync();
	};

	const auto on_raw_imu = [&](std::shared_ptr<MSP2Client::RawImu> imu) {
		pipe_0.handle(imu);
	};

	const auto on_att = [&](std::shared_ptr<MSP2Client::Attitude> att) {
		pipe_0.handle(att);
	};

	Player player(file_name);

	player.decode["msp.raw_imu"] 	= &MSP2Client::RawImu::read;
	player.decode["msp.attitude"] 	= &MSP2Client::Attitude::read;
	player.decode["msp.rc"] 		= &MSP2Client::RcPacket::read;
	player.decode["msp.raw_gps"] 	= &MSP2Client::RawGPS::read;

	player.handle["msp.raw_imu"]  = dispatch<MSP2Client::RawImu>(on_raw_imu);
	player.handle["msp.attitude"] = dispatch<MSP2Client::Attitude>(on_att);

	player.decode["camera.wide"] = &Image::read;
	player.handle["camera.wide"] = dispatch<Image>(on_frame_0);

	if(offset_sec > 0) {
		player.seek(offset_sec * 1000);
	}

	player.play();

	return 0;
}





