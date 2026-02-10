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

#include <mutex>
#include <iostream>

using namespace mmpilot;


int main(int argc, char** argv)
{
	const std::string file_name = argc > 1 ? argv[1] : "vapor1_record.dat";

	std::mutex mutex;

	DeBayer debayer_0;

	const auto on_frame = [&](std::shared_ptr<CameraFrame> frame)
	{
		std::lock_guard<std::mutex> lock(mutex);
		std::cout << "[" << frame->topic << "] ts = " << frame->timestamp
				<< ", width = " << frame->width << ", height = " << frame->height << std::endl;
	};

	const auto on_frame_1 = [&](std::shared_ptr<CameraFrame> frame)
	{
		on_frame(frame);
		debayer_0.handle(frame);
	};

	std::cout << "file_name = " << file_name << std::endl;

	auto ctx = EGL_create_context();

	GL_print_version();

	GL_compile_shader_file(GL_FRAGMENT_SHADER, "shader/color/luma.glsl");
	GL_compile_shader_file(GL_FRAGMENT_SHADER, "shader/test/test_interger_tex.glsl");

	render::init();

	Player player(file_name);

	player.decode["camera.front"] = &CameraFrame::read;
	player.decode["camera.below"] = &CameraFrame::read;

	player.handle["camera.front"] = type_cast<CameraFrame>(on_frame);
	player.handle["camera.below"] = type_cast<CameraFrame>(on_frame_1);

	player.play();

	render::cleanup();

	ctx.terminate();

	return 0;
}

