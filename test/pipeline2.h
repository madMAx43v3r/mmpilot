/*
 * pipeline2.h
 *
 *  Created on: Feb 23, 2026
 *      Author: mad
 */

#ifndef TEST_VAPOR1_PIPELINE2_H_
#define TEST_VAPOR1_PIPELINE2_H_

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
#include <mmpilot/virtual_cam.h>
#include <mmpilot/beta_msp.h>
#include <mmpilot/gyro.h>
#include <mmpilot/math.h>
#include <mmpilot/flow.h>
#include <mmpilot/merge.h>
#include <mmpilot/affine.h>

#include <mmpilot/egl.h>

using namespace mmpilot;


class Pipeline {
public:
	bool src_flip_x = false;
	bool src_flip_y = false;
	bool is_fisheye = true;
	bool is_debug = false;

	int64_t camera_delay = 50000;	// [us]

	float radius_mask = 1;			// proportional to width / 2

	float FOV_in = 200;				// fisheye deg (diagonal)
	float FOV_cam = 120;			// virtual deg (diagonal)
	float FOV_circle = 1;			// for FOV_in

	int cam_model = 3;				// (pinhole, equi-distant, equi-solid, stereo-graphic)
	Vec2f K_param = {0, 0};			// K2, K4

	Vec3f RPY_cam = Vec3f::Zero();	// relative to frame [deg]

	int pyramid_depth = 4;

	std::vector<int> num_iters = {1, 3, 7, 12};

	class Level {
	public:
		int level = 0;
		int num_smooth = -1;

		SmoothFilter smooth[3];
		GradientFilter gradient;
		Affine solver;

		Affine::Params A;

		std::shared_ptr<Level> upper;			// lower scale (upper level)
		std::shared_ptr<GL_Tex2D> base_img;

		void init(int level, int width, int height)
		{
			this->level = level;

			if(num_smooth < 0) {
				switch(level) {
					case 0:  num_smooth = 1; break;
					case 1:  num_smooth = 2; break;
					default: num_smooth = 3;
				}
			}
			for(int i = 0; i < num_smooth; ++i) {
				smooth[i].init(width, height, GL_RG16F, GL_RG, GL_HALF_FLOAT);
			}

			gradient.init(width, height);
			solver.init(width, height);

			base_img = std::make_shared<GL_Tex2D>(width, height, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT);

			fbo_copy[0] = GL_create_FBO(base_img->id);
			fbo_copy[1] = GL_create_FBO(gradient.out->id);
		}

		void exec(std::shared_ptr<GL_Tex2D> img)
		{
			if(have_base) {
				if(upper) {
					A = copy(upper->A).scale(2);
				}
				A = solver.exec(base_img, img, A);

				std::cout << "params[" << level << "][" << solver.num_iters << "] = " << to_string(A) << std::endl;
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

			glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo_copy[1]);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo_copy[0]);
			glBlitFramebuffer(
				0, 0, base_img->width, base_img->height,
				0, 0, base_img->width, base_img->height,
				GL_COLOR_BUFFER_BIT, GL_NEAREST
			);

			A = Affine::Params();
			have_base = true;
		}

	private:
		bool have_base = false;

		GLuint fbo_copy[2] = {};
	};

	Pipeline()
		:	gl_main(&Pipeline::gl_main_func)
	{
		gl_main.post([]{});
		sync();
	}

	virtual ~Pipeline()
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
	Gyro gyro_api;
	FlipImage flip_image;
	WeightRadius weight_radius;
	VirtualCam virtual_cam;
	WeightRadius virtual_weight_radius;
	PyramidFilter pyramid;

	MergeFilter merge;

	std::shared_ptr<GL_Tex2D> input_luma;

	std::shared_ptr<GL_Tex2D> source;	// source image for pyramid

	std::vector<std::shared_ptr<Level>> stage;

	Gyro::State gyro;

	int64_t ts = 0;

	Mat3f R_BC;			// camera to body
	Mat3f R_WB;			// body to world
	Mat3f R_EB;			// camera to extrinsic

	int in_width = 0;			// input to pipeline
	int in_height = 0;			// input to pipeline

	int src_width = 0;			// input to pyramid
	int src_height = 0;			// input to pyramid

	bool have_base = false;
	bool merge_init = false;

	std::unique_ptr<TexDisplay> display;

	virtual void init(int width, int height)
	{
		in_width = width;
		in_height = height;

		flip_image.flip_x = src_flip_x;
		flip_image.flip_y = src_flip_y;

		// shuffle matrix to make hand calibration easier
		// defaults to camera looking down, XY aligned to body frame
		R_EB <<  0,  1,  0,
				-1,  0,  0,
				 0,  0,  1;

//		R_BC = rpy_to_rot_zyx_deg(RPY_cam) * R_EB;
		R_BC = rpy_to_rot_zyx_deg(RPY_cam);

		if(radius_mask > 0) {
			weight_radius.radius = (width / 2) * radius_mask;
		}
		pyramid.depth = pyramid_depth;

		input_luma = std::make_shared<GL_Tex2D>(width, height, GL_R8, GL_RED, GL_UNSIGNED_BYTE);

		flip_image.init(width, height, GL_R8, GL_RED, GL_UNSIGNED_BYTE);

		weight_radius.init(GL_RED, width, height);

		if(is_fisheye) {
			virtual_cam.FOV_in = FOV_in;
			virtual_cam.FOV_cam = FOV_cam;
			virtual_cam.FOV_circle = FOV_circle;
			virtual_cam.K2 = K_param.x();
			virtual_cam.K4 = K_param.y();
			virtual_cam.model = cam_model;
			virtual_cam.init(GL_RG16F, GL_RG, GL_HALF_FLOAT);

			width = virtual_cam.width;
			height = virtual_cam.height;

			virtual_weight_radius.init(GL_RG, width, height);
		}
		src_width = width;
		src_height = height;

		pyramid.init(width, height, GL_RG16F, GL_RG, GL_HALF_FLOAT);

		merge.debug = is_debug;
		merge.weight = 0.1;
		merge.init(width, height, GL_RG);

		int w = width;
		int h = height;
		for(int i = 0; i < pyramid_depth; ++i)
		{
			auto lvl = std::make_shared<Level>();
			lvl->solver.debug = is_debug;
			lvl->solver.num_iters = num_iters[std::min(size_t(i), num_iters.size() - 1)];

			lvl->init(i, w, h);
			stage.push_back(lvl);

			w /= 2;
			h /= 2;
		}

		for(int i = 0; i + 1 < pyramid_depth; ++i) {
			stage[i]->upper = stage[i + 1];
		}
		have_init = true;
	}

	virtual void rebase()
	{
		for(int i = 0; i < pyramid_depth; ++i) {
			stage[i]->rebase(pyramid.out[i]);
		}
	}

	virtual void exec_filter(std::shared_ptr<GL_Tex2D> input)
	{
		flip_image.exec(input);

		weight_radius.exec(flip_image.out);

		source = weight_radius.out;
		if(is_fisheye) {
			virtual_cam.R_mat = R_BC * R_WB.transpose();
			virtual_cam.exec(source);

			virtual_weight_radius.exec(virtual_cam.out);
			source = virtual_weight_radius.out;
		}
		pyramid.exec(source);
	}

	virtual void exec(std::shared_ptr<GL_Tex2D> input)
	{
		if(!have_init) {
			throw std::logic_error("!have_init");
		}
		if(!gyro_api.avail()) {
			return;
		}
		gyro = gyro_api.lookup(ts);

		const Vec3f RPY = gyro.get_rpy();

//		R_WB = rpy_to_rot_zyx_deg<float>({RPY[1], -RPY[0], RPY[2]});
		R_WB = gyro.matrix();

		std::cout << "RPY = " << RPY[0] << " " << RPY[1] << " " << RPY[2] << std::endl;

		exec_filter(input);

		if(!have_base) {
			rebase();
			have_base = true;
			return;
		}

		// top down processing
		for(int i = pyramid_depth - 1; i >= 0; --i) {
			stage[i]->exec(pyramid.out[i]);
		}

		// back propagate most accurate result
		for(int i = 1; i < pyramid_depth; ++i) {
			stage[i]->A = copy(stage[i-1]->A).scale(0.5);
		}

		update();
	}

	virtual void update()
	{
		const auto base = merge_init ? merge.out_blend : source;

		merge.exec(base, source, get_params());

		merge_init = true;

//		show(display, merge.out, {1, 1, 1, 1});
		show(display, merge.tex_debug[0]);
//		show(display, merge.flow.stage[0]->flow[0].tex_debug);
//		show(display, stage[0]->base_img);
//		show(display, stage[0]->flow.tex_debug);
//		show(display, stage[0]->solver.tex_debug);

		rebase();
	}

	void reset(const Affine::Params& H)
	{
		stage[0]->A = H;

		for(int i = 1; i < pyramid_depth; ++i) {
			stage[i]->A = copy(stage[i-1]->A).scale(0.5);
		}
	}

	Affine::Params get_params(const int i = 0)
	{
		if(i < 0 || i >= pyramid_depth) {
			throw std::logic_error("get_params(): out of bounds");
		}
		return stage[i]->A;
	}

	void on_image(std::shared_ptr<Image> img)
	{
		const auto begin = get_time_micros();

		if(img->format == "JPEG") {
			int w, h;
			const auto& data = img->data[0];
			const auto img_luma = decode_jpeg_y(data.data(), data.size(), w, h);
			const auto img_rgba = decode_jpeg_rgba(data.data(), data.size(), w, h);

			std::cout << "[" << img->sequence << "] decode took " << (get_time_micros() - begin) / 1e3 << " ms" << std::endl;

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
		else {
			return;
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
		ts = (time_offset + img->timestamp)
				- img->exposure / 2
				- camera_delay;

		exec(input_luma);

		std::cout << "[" << img->sequence << "] total_time = " << (get_time_micros() - begin) / 1e3 << " ms" << std::endl;
	}

	virtual void on_sample(std::shared_ptr<Sample> sample)
	{
		if(auto img = std::dynamic_pointer_cast<Image>(sample)) {
			on_image(img);
		}
		else if(auto imu = std::dynamic_pointer_cast<MSP2Client::RawImu>(sample)) {
			gyro_api.on_raw_imu(*imu);
		}
		else if(auto att = std::dynamic_pointer_cast<MSP2Client::Attitude>(sample)) {
			gyro_api.on_attitude(*att);
		}
		else if(auto gps = std::dynamic_pointer_cast<MSP2Client::RawGPS>(sample)) {
			std::cout << "gps: lat=" << gps->lat << ", lon=" << gps->lon
					<< ", speed=" << gps->speed << ", heading=" << gps->course
					<< ", alt=" << gps->alt << ", sats=" << int(gps->num_sats) << ", fix=" << int(gps->fix_type) << std::endl;
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

	int64_t time_offset = 0;	// [us]

	bool have_init = false;
	bool time_init = false;

};






#endif /* TEST_VAPOR1_PIPELINE2_H_ */
