/*
 * pipeline.h
 *
 *  Created on: Feb 12, 2026
 *      Author: mad
 */

#ifndef TEST_VAPOR1_PIPELINE_H_
#define TEST_VAPOR1_PIPELINE_H_

#include <mmpilot/render.h>
#include <mmpilot/opengl.h>
#include <mmpilot/display.h>
#include <mmpilot/jpeg.h>
#include <mmpilot/image.h>
#include <mmpilot/weight.h>
#include <mmpilot/gradient.h>
#include <mmpilot/pyramid.h>

using namespace mmpilot;


class Pipeline {
public:
	int gradient_window = 7;
	int pyramid_depth = 6;

	WeightRadius weight_radius;
	PyramidFilter pyramid_filter;

	std::vector<std::shared_ptr<GradientFilter>> gradient_filters;

	std::shared_ptr<GL_Tex2D> input_luma;

	std::unique_ptr<TexDisplay> display;

	Pipeline()
		:	gl_main(&Pipeline::gl_main_func)
	{
		gl_main.post([]{});
		sync();
	}

	~Pipeline()
	{
		if(display) {
			display->close();
		}
		gl_main.close();
	}

	void handle(std::shared_ptr<Image> img)
	{
		gl_main.post(std::bind(&Pipeline::exec_image, this, img));
		sync();
	}

	void sync() {
		gl_main.sync();
	}

protected:
	void init(int width, int height)
	{
		this->width = width;
		this->height = height;

		pyramid_filter.depth = pyramid_depth;

		input_luma = std::make_shared<GL_Tex2D>(width, height, GL_R8, GL_RED, GL_UNSIGNED_BYTE);

		weight_radius.init(width, height);
		pyramid_filter.init(width, height, GL_RG16F, GL_RG, GL_HALF_FLOAT);

		int w = width;
		int h = height;
		for(int i = 0; i < pyramid_depth; ++i)
		{
			auto gradient = std::make_shared<GradientFilter>();
			gradient->win_size = gradient_window;
			gradient->init(w, h);
			gradient_filters.push_back(gradient);

			w /= 2;
			h /= 2;
		}
		have_init = true;
	}

	void exec()
	{
		if(!have_init) {
			throw std::logic_error("!have_init");
		}
		GL_finish();

		weight_radius.exec(input_luma);

		pyramid_filter.exec(weight_radius.out);

		for(int i = 0; i < pyramid_depth; ++i)
		{
			gradient_filters[i]->exec(pyramid_filter.out[i]);
		}

//		show(display, gradient_filters[3]->out, {1, 0, 0, 1});
	}

	void exec_image(std::shared_ptr<Image> img)
	{
		if(img->format == "JPEG") {
			int w, h;
			const auto& data = img->data[0];
			const auto img_luma = decode_jpeg_y(data.data(), data.size(), w, h);
			const auto img_rgba = decode_jpeg_rgba(data.data(), data.size(), w, h);

			if(!have_init) {
				init(w, h);
			}
			input_luma->upload(img_luma.data(), w);

//			show(display, img_rgba, w, h, 4);
		}
		else if(img->format == "YUV420") {
			if(!have_init) {
				init(img->width, img->height);
			}
			input_luma->upload(img->data[0].data(), img->stride);
		}
		exec();
	}

private:
	static void gl_main_func(Thread& self)
	{
		auto egl = EGL_create_context();

		GL_print_version();

		render::init();

		self.run();

		render::cleanup();

		egl.terminate();
	}

private:
	int width = 0;
	int height = 0;

	Thread gl_main;

	bool have_init = false;

};






#endif /* TEST_VAPOR1_PIPELINE_H_ */
