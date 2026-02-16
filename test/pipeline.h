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
#include <mmpilot/util.h>
#include <mmpilot/image.h>
#include <mmpilot/flip.h>
#include <mmpilot/weight.h>
#include <mmpilot/gradient.h>
#include <mmpilot/pyramid.h>
#include <mmpilot/smooth.h>
#include <mmpilot/homography.h>
#include <mmpilot/virtual_cam.h>
#include <mmpilot/beta_msp.h>
#include <mmpilot/gyro.h>
#include <mmpilot/math.h>

#include <mmpilot/egl.h>

using namespace mmpilot;


class Pipeline {
public:
	bool src_flip_x = false;
	bool src_flip_y = false;
	bool is_fisheye = true;

	float radius_mask = 1;			// proportional to width / 2

	float FOV_in = 200;				// fisheye deg (diagonal)
	float FOV_cam = 120;			// virtual deg (diagonal)

	Vec3f RPY_cam = Vec3f(0, 0, -35);	// relative to frame [deg]

	int gradient_window = 7;
	int pyramid_depth = 6;

	std::vector<int> num_iters = {1, 2, 4, 10, 20};

	Gyro gyro;
	FlipImage flip_image;
	WeightRadius weight_radius;
	VirtualCam virtual_cam;
	PyramidFilter pyramid_filter;

	class Level {
	public:
		int level = 0;

		SmoothFilter smooth[2];
		GradientFilter gradient;
		Homography solver;

		Homography::Params8 H_out;

		std::shared_ptr<Level> prev;			// lower scale (upper level)
		std::shared_ptr<GL_Tex2D> prev_img;

		void init(Pipeline* pipe, int level, int width, int height)
		{
			this->level = level;

			smooth[0].init(width, height, GL_RG16F, GL_RG, GL_HALF_FLOAT);
			smooth[1].init(width, height, GL_RG16F, GL_RG, GL_HALF_FLOAT);

			gradient.win_size = pipe->gradient_window;
			gradient.init(width, height);

			solver.init(width, height);

			prev_img = std::make_shared<GL_Tex2D>(width, height, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);

			glGenFramebuffers(2, fbo_tmp);
		}

		void exec(std::shared_ptr<GL_Tex2D> img, const Gyro::State& gyro)
		{
			smooth[0].exec(img);
			smooth[1].exec(smooth[0].out);

			gradient.exec(smooth[1].out);

			if(sequence) {
				Homography::Params8 p_init;
				if(prev) {
					p_init = prev->H_out;
					p_init.scale(2);
				}
				// TODO: handle exceptions
				H_out = solver.solve(prev_img, img, p_init);

				std::cout << "params[" << level << "][" << solver.num_iters << "] = " << to_string(H_out) << std::endl;
			}

			GL_blit_FBO(fbo_tmp[0], fbo_tmp[1], prev_img, gradient.out);

			sequence++;
		}

	private:
		uint64_t sequence = 0;
		GLuint fbo_tmp[2] = {};
	};

	std::vector<std::shared_ptr<Level>> stage;

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

	void handle(std::shared_ptr<Sample> sample)
	{
		gl_main.post(std::bind(&Pipeline::on_sample, this, sample));
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
		flip_image.flip_x = src_flip_x;
		flip_image.flip_y = src_flip_y;

		R_BC = rpy_to_rot_zyx_deg<Mat3f>(RPY_cam);

		Vec3f exB(1,0,0);
		std::cout << "roll axis in cam = " << (R_BC * exB).transpose() << std::endl;

		if(radius_mask > 0) {
			weight_radius.radius = (width / 2) * radius_mask;
		}
		pyramid_filter.depth = pyramid_depth;

		input_luma = std::make_shared<GL_Tex2D>(width, height, GL_R8, GL_RED, GL_UNSIGNED_BYTE);

		flip_image.init(width, height, GL_R8, GL_RED, GL_UNSIGNED_BYTE);
		weight_radius.init(width, height);

		if(is_fisheye) {
			virtual_cam.FOV_in = FOV_in;
			virtual_cam.FOV_cam = FOV_cam;
			virtual_cam.init(GL_RG16F, GL_RG, GL_HALF_FLOAT);
			width = virtual_cam.width;
			height = virtual_cam.height;
		}
		pyramid_filter.init(width, height, GL_RG16F, GL_RG, GL_HALF_FLOAT);

		int w = width;
		int h = height;
		for(size_t i = 0; i < pyramid_depth; ++i)
		{
			auto lvl = std::make_shared<Level>();
			lvl->solver.num_iters = num_iters[std::min(i, num_iters.size() - 1)];
			lvl->init(this, i, w, h);
			stage.push_back(lvl);
			w /= 2; h /= 2;
		}

		for(int i = 0; i + 1 < pyramid_depth; ++i)
		{
			stage[i]->prev = stage[i + 1];
		}

		have_init = true;
	}

	void exec(const int64_t ts)
	{
		if(!have_init) {
			throw std::logic_error("!have_init");
		}
		if(!gyro.avail()) {
			std::cout << "Waiting for gyro ..." << std::endl;
			return;
		}
		GL_finish();

		const auto gyro_state = gyro.lookup(ts);
		const auto RPY = gyro_state.RPY;

		std::cout << "RPY = " << RPY << std::endl;

		flip_image.exec(input_luma);

		weight_radius.exec(flip_image.out);

		if(is_fisheye) {
			const auto R_WB = rpy_to_rot_zyx_deg<Mat3f>(Vec3f(RPY[1], -RPY[0], -RPY[2]));
			virtual_cam.R_mat = R_BC * R_WB.transpose();
			virtual_cam.exec(weight_radius.out);
			pyramid_filter.exec(virtual_cam.out);
		} else {
			pyramid_filter.exec(weight_radius.out);
		}

		for(int i = pyramid_depth - 1; i >= 0; --i)
		{
			stage[i]->exec(pyramid_filter.out[i], gyro_state);
		}

		prev_gyro = gyro_state;

//		show(display, flip_image.out, {1, 0.2, 1, 1});
		show(display, virtual_cam.out, {1, 0.1, 1, 1});
//		show(display, pyramid_filter.out[5], {1, 0.5, 1, 1});
//		show(display, stage[2]->smooth[1].out, {1, 0.1, 1, 1});
//		show(display, stage[0]->solver.tex_debug, {1, 1, 1, 1});
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
		exec(img->ts);
	}

	void on_sample(std::shared_ptr<Sample> sample)
	{
		if(auto imu = std::dynamic_pointer_cast<MSP2Client::RawImu>(sample)) {
			gyro.on_raw_imu(*imu);
		}
		else if(auto att = std::dynamic_pointer_cast<MSP2Client::Attitude>(sample)) {
			gyro.on_attitude(*att);
		}
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
	Thread gl_main;

	Mat3f K_cam;			// intrinsic
	Mat3f R_BC;			// mounting to frame

	Gyro::State prev_gyro;

	bool have_init = false;

};






#endif /* TEST_VAPOR1_PIPELINE_H_ */
