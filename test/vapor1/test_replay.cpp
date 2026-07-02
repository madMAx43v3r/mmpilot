/*
 * test_replay.cpp
 *
 *  Created on: Feb 9, 2026
 *      Author: mad
 */

#include <mmpilot/replay.h>
#include <mmpilot/image.h>
#include <mmpilot/navigation.h>

#include <iostream>


int main(int argc, char** argv)
{
	const int64_t offset_sec = argc > 1 ? atoi(argv[1]) : 0;
	const std::string file_name = argc > 2 ? argv[2] : "vapor1_record.dat";

	std::cout << "offset = " << offset_sec << " sec" << std::endl;
	std::cout << "file_name = " << file_name << std::endl;

	Navigation nav(nullptr);
	nav.pipe.src_flip_y = true;
	nav.pipe.radius_mask = 0.9;
	nav.virtual_cam.FOV_in = 190;
	nav.virtual_cam.FOV_cam = 120;
	nav.virtual_cam.RPY_cam = Vec3f(0, 0, -30 -90);
	nav.virtual_cam.cam_model = 3;
	nav.virtual_cam.K_param  = Vec2f(-0.01, -0.01);		// stereo


	const auto on_frame = [&](std::shared_ptr<Image> frame) {
		std::cout << "[" << frame->topic << "] ts = " << frame->timestamp
				<< ", width = " << frame->width << ", height = " << frame->height
				<< ", size = " << frame->data.size()
				<< ", exposure = " << frame->exposure << ", gain = " << frame->analog_gain << std::endl;
		nav.pipe.handle(frame);
		nav.pipe.sync();
	};

	const auto on_raw_imu = [&](std::shared_ptr<MSP2::RawImu> imu) {
		nav.pipe.handle(imu);
	};

	const auto on_att = [&](std::shared_ptr<MSP2::Attitude> att) {
		nav.pipe.handle(att);
	};

	const auto on_gps = [&](std::shared_ptr<MSP2::RawGPS> gps) {
		nav.pipe.handle(gps);
	};

	const auto on_alt = [&](std::shared_ptr<MSP2::Altitude> alt) {
		nav.pipe.handle(alt);
	};

	const auto on_rc = [&](std::shared_ptr<MSP2::RcPacket> rc) {
		nav.pipe.handle(rc);
	};

	Player player(file_name);

	player.decode["msp.raw_imu"] 	= &MSP2::RawImu::read;
	player.decode["msp.attitude"] 	= &MSP2::Attitude::read;
	player.decode["msp.altitude"] 	= &MSP2::Altitude::read;
	player.decode["msp.raw_gps"] 	= &MSP2::RawGPS::read;
	player.decode["msp.rc"] 		= &MSP2::RcPacket::read;

	player.handle["msp.raw_imu"]  = dispatch<MSP2::RawImu>(on_raw_imu);
	player.handle["msp.attitude"] = dispatch<MSP2::Attitude>(on_att);
	player.handle["msp.altitude"] = dispatch<MSP2::Altitude>(on_alt);
	player.handle["msp.raw_gps"]  = dispatch<MSP2::RawGPS>(on_gps);
	player.handle["msp.rc"]       = dispatch<MSP2::RcPacket>(on_rc);

	player.decode["camera.nav"] = &Image::read;
	player.decode["camera.wide"] = &Image::read;

	player.handle["camera.nav"] = dispatch<Image>(on_frame);
	player.handle["camera.wide"] = dispatch<Image>(on_frame);

	if(offset_sec > 0) {
		player.seek(offset_sec * 1000);
	}

	player.speed = 1;
	player.real_time = true;

	player.play();

	return 0;
}





