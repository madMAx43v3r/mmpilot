/*
 * test_replay.cpp
 *
 *  Created on: Feb 9, 2026
 *      Author: mad
 */

#include <mmpilot/replay.h>
#include <mmpilot/sample.h>
#include <mmpilot/camera_frame.h>

#include <mutex>
#include <iostream>

using namespace mmpilot;


int main(int argc, char** argv)
{
	const std::string file_name = argc > 1 ? argv[1] : "vapor1_record.dat";

	std::mutex mutex;

	const auto on_frame = [&](std::shared_ptr<CameraFrame> frame)
	{
		{
			std::lock_guard<std::mutex> lock(mutex);
			std::cout << "[" << frame->topic << "] ts = " << frame->timestamp
					<< ", width = " << frame->width << ", height = " << frame->height << std::endl;
		}
	};

	std::cout << "file_name = " << file_name << std::endl;

	Player player(file_name, 4);

	player.decode["camera.front"] = &CameraFrame::read;
	player.decode["camera.below"] = &CameraFrame::read;

	player.handle["camera.front"] = type_cast<CameraFrame>(on_frame);
	player.handle["camera.below"] = type_cast<CameraFrame>(on_frame);

	player.play();

	return 0;
}

