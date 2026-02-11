/*
 * test_replay.cpp
 *
 *  Created on: Feb 9, 2026
 *      Author: mad
 */

#include <mmpilot/replay.h>
#include <mmpilot/sample.h>
#include <mmpilot/render.h>
#include <mmpilot/egl.h>
#include <mmpilot/opengl.h>
#include <mmpilot/thread.h>
#include <mmpilot/display.h>
#include <mmpilot/jpeg.h>
#include <mmpilot/image.h>

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

	std::unique_ptr<TexDisplay> display;

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

		int w, h;
		const auto luma = decode_jpeg_y(frame->data.data(), frame->data.size(), w, h);
		const auto rgba = decode_jpeg_rgba(frame->data.data(), frame->data.size(), w, h);

//		if(!display) {
//			display = std::make_unique<TexDisplay>(w, h);
//		}
//		display->show(rgba);
	};

	const auto on_frame_1 = [&](std::shared_ptr<Image> frame)
	{
		on_frame(frame);

		int w, h;
		const auto luma = decode_jpeg_y(frame->data.data(), frame->data.size(), w, h);
		const auto rgba = decode_jpeg_rgba(frame->data.data(), frame->data.size(), w, h);

		if(!display) {
			display = std::make_unique<TexDisplay>(w, h);
		}
		display->show(rgba);
	};

	Player player(file_name);

	player.decode["camera.front"] = &Image::read;
	player.decode["camera.below"] = &Image::read;

	player.handle["camera.front"] = dispatch<Image>(gl_main, on_frame_0);
	player.handle["camera.below"] = dispatch<Image>(gl_main, on_frame_1);

	player.play();

	if(display) {
		display->close();
	}
	gl_main.close();

	return 0;
}





