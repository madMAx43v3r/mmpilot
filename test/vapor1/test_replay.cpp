/*
 * test_replay.cpp
 *
 *  Created on: Feb 9, 2026
 *      Author: mad
 */

#include <mmpilot/replay.h>
#include <mmpilot/camera_frame.h>

#include <mutex>
#include <iostream>

using namespace mmpilot;


int main(int argc, char** argv)
{
	const std::string file_name = argc > 1 ? argv[1] : "vapor1_record.dat";

	std::mutex mutex;

	const auto on_frame = [&](Player& in, const std::string& topic)
	{
		auto frame = CameraFrame::read(in);
		{
			std::lock_guard<std::mutex> lock(mutex);
			std::cout << "[" << topic << "] ts = " << frame->timestamp
					<< ", width = " << frame->width << ", height = " << frame->height << std::endl;
		}
	};

	Player player(file_name, 4);

	player.handle["camera.front"] = on_frame;
	player.handle["camera.below"] = on_frame;

	return 0;
}

