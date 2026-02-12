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
#include <mmpilot/weight.h>

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

class Pipeline {
public:
	WeightRadius weight_radius;

	std::unique_ptr<TexDisplay> display;

	~Pipeline()
	{
		if(display) {
			display->close();
		}
	}

	void init(int width, int height)
	{
		this->width = width;
		this->height = height;

		luma = std::make_shared<GL_Tex2D>(width, height, GL_R8, GL_RED, GL_UNSIGNED_BYTE);

		weight_radius.init(width, height);

		have_init = true;
	}

	void exec()
	{
		if(!have_init) {
			throw std::logic_error("!have_init");
		}
		weight_radius.exec(luma);

		show(display, weight_radius.out);
	}

	void exec_jpeg(std::shared_ptr<Image> frame)
	{
		int w, h;
		const auto img_y = decode_jpeg_y(frame->data.data(), frame->data.size(), w, h);

		if(!have_init) {
			init(w, h);
		}
		luma->upload(img_y.data(), w);
		exec();
	}

private:
	int width = 0;
	int height = 0;

	std::shared_ptr<GL_Tex2D> luma;

	bool have_init = false;
};

int main(int argc, char** argv)
{
	const std::string file_name = argc > 1 ? argv[1] : "vapor1_record.dat";

	std::cout << "file_name = " << file_name << std::endl;

	Thread gl_main(&gl_main_func);

	Pipeline pipe_0;
	Pipeline pipe_1;

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
		const auto& data = frame->data;
		const auto luma = decode_jpeg_y(data.data(), data.size(), w, h);
		const auto rgba = decode_jpeg_rgba(data.data(), data.size(), w, h);

		gl_main.post(std::bind(&Pipeline::exec_jpeg, &pipe_0, frame));

//		show(display, luma, w, h, 1);
//		show(display, rgba, w, h, 4);
	};

	const auto on_frame_1 = [&](std::shared_ptr<Image> frame)
	{
		on_frame(frame);

		int w, h;
		const auto& data = frame->data;
		const auto luma = decode_jpeg_y(data.data(), data.size(), w, h);
		const auto rgba = decode_jpeg_rgba(data.data(), data.size(), w, h);

		gl_main.post(std::bind(&Pipeline::exec_jpeg, &pipe_1, frame));

//		show(display, luma, w, h, 1);
//		show(display, rgba, w, h, 4);
	};

	Player player(file_name);

	player.decode["camera.front"] = &Image::read;
	player.decode["camera.below"] = &Image::read;

	player.handle["camera.front"] = dispatch<Image>(gl_main, on_frame_0);
	player.handle["camera.below"] = dispatch<Image>(gl_main, on_frame_1);

	player.play();

	gl_main.close();

	return 0;
}





