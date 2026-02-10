/*
 * test_replay.cpp
 *
 *  Created on: Feb 9, 2026
 *      Author: mad
 */

#include <mmpilot/replay.h>
#include <mmpilot/sample.h>
#include <mmpilot/camera_frame.h>
#include <mmpilot/render.h>
#include <mmpilot/bayer.h>
#include <mmpilot/egl.h>
#include <mmpilot/opengl.h>
#include <mmpilot/thread.h>

#include <mutex>
#include <iostream>

using namespace mmpilot;


void gl_main(Thread& self)
{
	auto ctx = EGL_create_context();

	GL_print_version();

	render::init();

	self.run();

	render::cleanup();

	ctx.terminate();
}

int main(int argc, char** argv)
{
	const std::string file_name = argc > 1 ? argv[1] : "vapor1_record.dat";

	std::cout << "file_name = " << file_name << std::endl;

	Thread thread(&gl_main);

	DeBayer debayer_1;

	const auto on_frame = [&](std::shared_ptr<CameraFrame> frame)
	{
		std::cout << "[" << frame->topic << "] ts = " << frame->timestamp
				<< ", width = " << frame->width << ", height = " << frame->height << std::endl;
	};

	const auto on_frame_1 = [&](std::shared_ptr<CameraFrame> frame)
	{
		on_frame(frame);
		debayer_1.handle(frame);
	};

	debayer_1.on_luma = [&](std::shared_ptr<GL_Tex2D> luma)
	{
		std::cout << "Got luma: width = " << luma->width << ", height = " << luma->height << std::endl;
	};

	debayer_1.on_rgba = [&](std::shared_ptr<GL_Tex2D> luma)
	{
		std::cout << "Got rgba: width = " << luma->width << ", height = " << luma->height << std::endl;
	};

	Player player(file_name);

	player.decode["camera.front"] = &CameraFrame::read;
	player.decode["camera.below"] = &CameraFrame::read;

	player.handle["camera.front"] = dispatch<CameraFrame>(thread, on_frame);
	player.handle["camera.below"] = dispatch<CameraFrame>(thread, on_frame_1);

	player.play();

	thread.close();

	return 0;
}

