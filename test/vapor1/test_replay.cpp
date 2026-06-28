/*
 * test_replay.cpp
 *
 *  Created on: Feb 9, 2026
 *      Author: mad
 */

#include <mmpilot/replay.h>
#include <mmpilot/image.h>

#include "../pipeline2.h"
#include "../mapping.h"
#include "../localization.h"
#include "../test_control.h"

#include <iostream>


int main(int argc, char** argv)
{
	const int64_t offset_sec = argc > 1 ? atoi(argv[1]) : 0;
	const std::string file_name = argc > 2 ? argv[2] : "vapor1_record.dat";

	std::cout << "offset = " << offset_sec << " sec" << std::endl;
	std::cout << "file_name = " << file_name << std::endl;

//	Pipeline pipe_0;
//	MappingPipe pipe_0;
	TestControl pipe_0(nullptr);
//	LocalizationPipe pipe_0("map.dat");
//	pipe_0.mapping.gps_alt_override = 150;
//	CalibrationPipe pipe_0;
	pipe_0.is_debug = true;
	pipe_0.src_flip_y = true;
	pipe_0.radius_mask = 0.9;
	pipe_0.FOV_in = 190;
	pipe_0.FOV_cam = 120;
	pipe_0.RPY_cam = Vec3f(0, 0, -30 -90);
	pipe_0.cam_model = 3;
	pipe_0.K_param  = Vec2f(-0.01, -0.01);		// stereo
//	pipe_0.K_param  = Vec2f(0.15, 0.01);	// equidistant

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

	const auto on_gps = [&](std::shared_ptr<MSP2Client::RawGPS> gps) {
		pipe_0.handle(gps);
	};

	const auto on_alt = [&](std::shared_ptr<MSP2Client::Altitude> alt) {
		pipe_0.handle(alt);
	};

	const auto on_rc = [&](std::shared_ptr<MSP2Client::RcPacket> rc) {
		pipe_0.handle(rc);
	};

	Player player(file_name);

	player.decode["msp.raw_imu"] 	= &MSP2Client::RawImu::read;
	player.decode["msp.attitude"] 	= &MSP2Client::Attitude::read;
	player.decode["msp.altitude"] 	= &MSP2Client::Altitude::read;
	player.decode["msp.raw_gps"] 	= &MSP2Client::RawGPS::read;
	player.decode["msp.rc"] 		= &MSP2Client::RcPacket::read;

	player.handle["msp.raw_imu"]  = dispatch<MSP2Client::RawImu>(on_raw_imu);
	player.handle["msp.attitude"] = dispatch<MSP2Client::Attitude>(on_att);
	player.handle["msp.altitude"] = dispatch<MSP2Client::Altitude>(on_alt);
	player.handle["msp.raw_gps"]  = dispatch<MSP2Client::RawGPS>(on_gps);
	player.handle["msp.rc"]       = dispatch<MSP2Client::RcPacket>(on_rc);

	player.decode["camera.wide"] = &Image::read;
	player.handle["camera.wide"] = dispatch<Image>(on_frame_0);

	if(offset_sec > 0) {
		player.seek(offset_sec * 1000);
	}

	player.speed = 1;
	player.real_time = true;

	player.play();

	return 0;
}





