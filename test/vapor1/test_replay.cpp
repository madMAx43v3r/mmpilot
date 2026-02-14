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
	const std::string file_name = argc > 1 ? argv[1] : "vapor1_record.dat";

	std::cout << "file_name = " << file_name << std::endl;

	Pipeline pipe_0;
	pipe_0.src_flip_y = true;
	pipe_0.radius_mask = 0.9;

	const auto on_frame = [&](std::shared_ptr<Image> frame)
	{
		std::cout << "[" << frame->topic << "] ts = " << frame->timestamp
				<< ", width = " << frame->width << ", height = " << frame->height
				<< ", size = " << frame->data.size()
				<< ", exposure = " << frame->exposure << ", gain = " << frame->analog_gain << std::endl;
	};

	const auto on_frame_0 = [&](std::shared_ptr<Image> frame)
	{
		on_frame(frame);
		pipe_0.handle(frame);
	};

	Player player(file_name);

	player.decode["camera.wide"] = &Image::read;

	player.handle["camera.wide"] = dispatch<Image>(on_frame_0);

	player.play();

	return 0;
}





