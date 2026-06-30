/*
 * export_images.cpp
 *
 *  Created on: Feb 23, 2026
 *      Author: mad
 */

#include <mmpilot/replay.h>
#include <mmpilot/image.h>
#include <mmpilot/beta_msp.h>

#include <iostream>
#include <fstream>

using namespace mmpilot;


int main(int argc, char** argv)
{
	const int64_t offset_sec = argc > 1 ? atoi(argv[1]) : 0;
	const std::string file_name = argc > 2 ? argv[2] : "vapor1_record.dat";

	std::cout << "offset = " << offset_sec << " sec" << std::endl;
	std::cout << "file_name = " << file_name << std::endl;

	size_t counter = 0;

	const auto on_frame = [&](std::shared_ptr<Image> frame) {
		if(frame->exposure > 40000) {
			return;
		}
		std::cout << "[" << frame->topic << "] ts = " << frame->timestamp
				<< ", width = " << frame->width << ", height = " << frame->height
				<< ", size = " << frame->data.size()
				<< ", exposure = " << frame->exposure << ", gain = " << frame->analog_gain << std::endl;

		if(frame->format == "JPEG") {
			const auto& data = frame->data[0];

			std::ofstream f("images/" + frame->topic + "_" + std::to_string(counter) + ".jpeg");
			f.write((const char*)data.data(), data.size());
			f.close();

			counter++;
		}
	};

	const auto on_frame_0 = [&](std::shared_ptr<Image> frame) {
		on_frame(frame);
	};

	Player player(file_name);
	player.real_time = false;

	player.decode["msp.raw_imu"] 	= &MSP2::RawImu::read;
	player.decode["msp.attitude"] 	= &MSP2::Attitude::read;
	player.decode["msp.rc"] 		= &MSP2::RcPacket::read;
	player.decode["msp.raw_gps"] 	= &MSP2::RawGPS::read;

	player.decode["camera.wide"] = &Image::read;
	player.handle["camera.wide"] = dispatch<Image>(on_frame_0);

	if(offset_sec > 0) {
		player.seek(offset_sec * 1000);
	}

	player.play();

	return 0;
}








