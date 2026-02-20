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
#include <mmpilot/mapping.h>

#include <mmpilot/egl.h>

using namespace mmpilot;


class Pipeline {
public:
	bool src_flip_x = false;
	bool src_flip_y = false;
	bool is_fisheye = true;

	int64_t camera_delay = 20000;	// [us]

	float radius_mask = 1;			// proportional to width / 2
	float rebase_delta = 20;		// pixels traveled
	float rebase_scale = 1.33;		// relative scale change

	float FOV_in = 200;				// fisheye deg (diagonal)
	float FOV_cam = 120;			// virtual deg (diagonal)
	float FOV_circle = 0.9;			// for FOV_in

	Vec3f RPY_cam = Vec3f(0, 0, -35);	// relative to frame [deg]

	int gradient_window = 7;
	int pyramid_depth = 5;

	std::vector<int> num_iters = {1, 2, 4, 8, 12};

	Gyro gyro;
	FlipImage flip_image;
	WeightRadius weight_radius;
	VirtualCam virtual_cam;
	PyramidFilter pyramid;
	Mapping mapping;

	class Level {
	public:
		int level = 0;
		int num_smooth = -1;

		SmoothFilter smooth[3];
		GradientFilter gradient;
		Homography solver;

		Homography::Params H;

		std::shared_ptr<Level> upper;			// lower scale (upper level)
		std::shared_ptr<GL_Tex2D> base_img;

		void init(Pipeline* pipe, int level, int width, int height)
		{
			this->level = level;

			if(num_smooth < 0) {
				switch(level) {
					case 0:  num_smooth = 0; break;
					case 1:  num_smooth = 1; break;
					case 2:  num_smooth = 2; break;
					default: num_smooth = 3;
				}
			}
			for(int i = 0; i < num_smooth; ++i) {
				smooth[i].init(width, height, GL_RG16F, GL_RG, GL_HALF_FLOAT);
			}

			gradient.win_size = level > 0 ? pipe->gradient_window : 5;
			gradient.init(width, height);

			solver.init(width, height);

			base_img = std::make_shared<GL_Tex2D>(width, height, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);

			glGenFramebuffers(2, fbo_tmp);
		}

		void exec(std::shared_ptr<GL_Tex2D> img)
		{
			if(have_base) {
				if(upper) {
					H = Homography::Params(upper->H).scale(2);
				}
				H = solver.solve(base_img, img, H);

				std::cout << "params[" << level << "][" << solver.num_iters << "] = " << to_string(H) << std::endl;
			} else {
				rebase(img);
			}
		}

		void rebase(std::shared_ptr<GL_Tex2D> img)
		{
			auto in = img;
			for(int i = 0; i < num_smooth; ++i) {
				smooth[i].exec(in);
				in = smooth[i].out;
			}
			gradient.exec(in);

			GL_blit_FBO(fbo_tmp[0], fbo_tmp[1], base_img, gradient.out);

			H = Homography::Params();
			have_base = true;
		}

	private:
		bool have_base = false;

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

	void handle(std::shared_ptr<Image> img)
	{
		gl_main.post(std::bind(&Pipeline::on_image, this, img));
	}

	void handle(std::shared_ptr<Sample> sample)
	{
		gl_main.post(std::bind(&Pipeline::on_sample, this, sample));
	}

	void sync() {
		gl_main.sync();
	}

protected:
	void init(int width, int height)
	{
		flip_image.flip_x = src_flip_x;
		flip_image.flip_y = src_flip_y;

		R_BC = rpy_to_rot_zyx_deg(RPY_cam);

		if(radius_mask > 0) {
			weight_radius.radius = (width / 2) * radius_mask;
		}
		pyramid.depth = pyramid_depth;

		input_luma = std::make_shared<GL_Tex2D>(width, height, GL_R8, GL_RED, GL_UNSIGNED_BYTE);

		flip_image.init(width, height, GL_R8, GL_RED, GL_UNSIGNED_BYTE);
		weight_radius.init(width, height);

		if(is_fisheye) {
			virtual_cam.FOV_in = FOV_in;
			virtual_cam.FOV_cam = FOV_cam;
			virtual_cam.FOV_circle = FOV_circle;
			virtual_cam.init(GL_RG16F, GL_RG, GL_HALF_FLOAT);
			width = virtual_cam.width;
			height = virtual_cam.height;
		}
		pyramid.init(width, height, GL_RG16F, GL_RG, GL_HALF_FLOAT);

		int w = width;
		int h = height;
		for(int i = 0; i < pyramid_depth; ++i)
		{
			auto lvl = std::make_shared<Level>();
			lvl->solver.num_iters = num_iters[std::min(size_t(i), num_iters.size() - 1)];
			lvl->init(this, i, w, h);
			stage.push_back(lvl);
			w /= 2; h /= 2;
		}

		for(int i = 0; i + 1 < pyramid_depth; ++i)
		{
			stage[i]->upper = stage[i + 1];
		}

		mapping.init(width * 2, height * 2, GL_RG);

		have_init = true;
	}

	void exec(const int64_t ts)
	{
		if(!have_init) {
			throw std::logic_error("!have_init");
		}
		if(!gyro.avail()) {
			return;
		}
		Gyro::State gyro_now;
		try {
			gyro_now = gyro.lookup(ts);
		} catch(std::exception& ex) {
			std::cout << "Gyro failed with: " << ex.what() << std::endl;
		}
		const Vec3f RPY = gyro_now.get_rpy();

		std::cout << "RPY = " << RPY[0] << " " << RPY[1] << " " << RPY[2] << std::endl;

		flip_image.exec(input_luma);

		weight_radius.exec(flip_image.out);

		auto src = weight_radius.out;
		if(is_fisheye) {
			const auto R_WB = rpy_to_rot_zyx_deg<float>({RPY[1], -RPY[0], -RPY[2]});
			virtual_cam.R_mat = R_BC * R_WB.transpose();
			virtual_cam.exec(weight_radius.out);
			src = virtual_cam.out;
		}
		pyramid.exec(src);

		// TODO: handle exceptions
		for(int i = pyramid_depth - 1; i >= 0; --i) {
			// top down processing
			stage[i]->exec(pyramid.out[i]);
		}

		for(int i = 1; i < pyramid_depth; ++i) {
			// back propagate most accurate result
			stage[i]->H = Homography::Params(stage[i-1]->H).scale(0.5);
		}
		const auto H = stage[0]->H;
		const auto T = H.transform();

		std::cout << "homography: R_norm = " << H.R_norm << ", overlap = " << H.overlap << std::endl;

		if(T.pos.norm() > rebase_delta || T.scale > rebase_scale || T.scale < 1 / rebase_scale)
		{
			for(int i = 0; i < pyramid_depth; ++i) {
				stage[i]->rebase(pyramid.out[i]);
			}
			mapping.update(T);

			base_gyro = gyro_now;
			have_base = true;
		}
		mapping.render(src, H);

//		const auto map = mapping.finalize();		// TODO: debugging

//		show(display, flip_image.out, {1, 0.2, 1, 1});
//		show(display, virtual_cam.out, {1, 0.1, 1, 1});
//		show(display, pyramid_filter.out[5], {1, 0.5, 1, 1});
//		show(display, stage[2]->smooth[1].out, {1, 0.1, 1, 1});
//		show(display, stage[0]->solver.tex_debug, {1, 1, 1, 1});
	}

	void on_image(std::shared_ptr<Image> img)
	{
		const auto begin = get_time_micros();

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

		{
			const auto ts_off = img->ts - img->timestamp;
			if(time_init) {
				// keep updating offset to converge
				time_offset = (time_offset * 63 + ts_off) / 64;
			} else {
				time_offset = ts_off;
				time_init = true;
			}
			std::cout << "time_offset = " << time_offset << std::endl;
		}
		const auto ts = (time_offset + img->timestamp)
				- img->exposure / 2
				- camera_delay;

		exec(ts);

		std::cout << "[" << img->sequence << "] total_time = " << (get_time_micros() - begin) / 1e3 << " ms" << std::endl;
	}

	void on_sample(std::shared_ptr<Sample> sample)
	{
		if(auto img = std::dynamic_pointer_cast<Image>(sample)) {
			on_image(img);
		}
		else if(auto imu = std::dynamic_pointer_cast<MSP2Client::RawImu>(sample)) {
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

	Mat3f R_BC;			// mounting to frame

	Gyro::State base_gyro;

	int64_t time_offset = 0;	// [us]

	bool have_init = false;
	bool time_init = false;
	bool have_base = false;

};






#endif /* TEST_VAPOR1_PIPELINE_H_ */
