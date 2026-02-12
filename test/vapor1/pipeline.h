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

using namespace mmpilot;


class Pipeline {
public:
	std::shared_ptr<GL_Tex2D> input_luma;

	WeightRadius weight_radius;

	GradientFilter gradient_filter;

	std::unique_ptr<TexDisplay> display;

	Pipeline()
		:	gl_main(&Pipeline::gl_main_func)
	{
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
	}

protected:
	void init(int width, int height)
	{
		this->width = width;
		this->height = height;

		input_luma = std::make_shared<GL_Tex2D>(width, height, GL_R8, GL_RED, GL_UNSIGNED_BYTE);

		weight_radius.init(width, height);
		gradient_filter.init(width, height);

		have_init = true;
	}

	void exec()
	{
		if(!have_init) {
			throw std::logic_error("!have_init");
		}
		weight_radius.exec(input_luma);

		gradient_filter.exec(weight_radius.out);

		show(display, gradient_filter.out, {0, 1, 1, 1});
	}

	void exec_image(std::shared_ptr<Image> img)
	{
		if(img->format == "JPEG") {
			int w, h;
			const auto& data = img->data[0];
			const auto img_y = decode_jpeg_y(data.data(), data.size(), w, h);

			if(!have_init) {
				init(w, h);
			}
			input_luma->upload(img_y.data(), w);
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
