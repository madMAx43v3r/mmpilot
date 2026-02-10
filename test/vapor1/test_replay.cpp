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
#include <mmpilot/display.h>

#include <mutex>
#include <memory>
#include <iostream>
#include <condition_variable>

using namespace mmpilot;


void gl_main_func(Thread& self)
{
	auto egl = EGL_create_context();

	GL_print_version();

	render::init();

	self.run();

	render::cleanup();

	egl.terminate();
}

int main(int argc, char** argv)
{
	const std::string file_name = argc > 1 ? argv[1] : "vapor1_record.dat";

	std::cout << "file_name = " << file_name << std::endl;

	Thread gl_main(&gl_main_func);

	DeBayer debayer_0;
	DeBayer debayer_1;

	std::unique_ptr<TexDisplay> display;

	const auto on_frame = [&](std::shared_ptr<CameraFrame> frame)
	{
		std::cout << "[" << frame->topic << "] ts = " << frame->timestamp
				<< ", width = " << frame->width << ", height = " << frame->height << std::endl;
	};

	const auto on_frame_0 = [&](std::shared_ptr<CameraFrame> frame)
	{
		on_frame(frame);
		debayer_0.handle(frame);
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

	debayer_1.on_rgba = [&](std::shared_ptr<GL_Tex2D> rgba)
	{
		std::cout << "Got rgba: width = " << rgba->width << ", height = " << rgba->height << std::endl;

		if(!display) {
			display = std::make_unique<TexDisplay>(rgba->width, rgba->height);
		}
		display->show(rgba);
	};

	Player player(file_name);

	player.decode["camera.front"] = &CameraFrame::read;
	player.decode["camera.below"] = &CameraFrame::read;

	player.handle["camera.front"] = dispatch<CameraFrame>(gl_main, on_frame_0);
	player.handle["camera.below"] = dispatch<CameraFrame>(gl_main, on_frame_1);

	player.play();

	if(display) {
		display->close();
	}
	gl_main.close();

	return 0;
}





