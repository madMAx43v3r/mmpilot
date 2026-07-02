/*
 * pipeline.h
 *
 *  Created on: Jun 29, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_PIPELINE_H_
#define INCLUDE_MMPILOT_PIPELINE_H_

#include <mmpilot/render.h>
#include <mmpilot/opengl.h>
#include <mmpilot/jpeg.h>
#include <mmpilot/util.h>
#include <mmpilot/image.h>
#include <mmpilot/flip.h>
#include <mmpilot/weight.h>
#include <mmpilot/beta_msp.h>
#include <mmpilot/gyro.h>
#include <mmpilot/math.h>
#include <mmpilot/stage.h>
#include <mmpilot/gps.h>
#include <mmpilot/record.h>

#include <mmpilot/egl.h>

using namespace mmpilot;


class Pipeline : public Stage {
public:
	bool src_flip_x = false;
	bool src_flip_y = false;

	bool is_debug = false;
	bool do_record = false;

	int64_t camera_delay = 50000;	// [us]

	float radius_mask = 1;			// proportional to width / 2


	Pipeline()
		:	Stage("root"),
			gl_main(&Pipeline::gl_main_func)
	{
		// wait for opengl init
		gl_main.post([]{});
		sync();
	}

	virtual ~Pipeline()
	{
		if(recorder) {
			recorder->close();
		}
		gl_main.close();
	}

	void handle(std::shared_ptr<Sample> sample)
	{
		gl_main.post(std::bind(&Pipeline::on_sample, this, sample));
	}

	void sync() {
		gl_main.sync();
	}

	void init_sync(int width, int height) {
		gl_main.post(std::bind(&Pipeline::init_wh, this, width, height));
		sync();
	}

	void add_stage(Stage* stage)
	{
		if(exec_chain.empty()) {
			stage->prev_stage = this;
		} else {
			stage->prev_stage = exec_chain.back();
		}
		exec_chain.push_back(stage);
	}

	void connect(MSP2* msp)
	{
		msp->on_raw_imu = [this](const MSP2::RawImu& imu) {
			handle(std::make_shared<MSP2::RawImu>(imu));
		};
		msp->on_attitude = [this](const MSP2::Attitude& att) {
			handle(std::make_shared<MSP2::Attitude>(att));
		};
		msp->on_altitude = [this](const MSP2::Altitude& alt) {
			handle(std::make_shared<MSP2::Altitude>(alt));
		};
		msp->on_rc = [this](const MSP2::RcPacket& rc) {
			handle(std::make_shared<MSP2::RcPacket>(rc));
		};
		msp->on_gps = [this](const MSP2::RawGPS& gps) {
			handle(std::make_shared<MSP2::RawGPS>(gps));
		};
	}

	// ---- Stage members ----

	int width = 0;			// input to pipeline
	int height = 0;			// input to pipeline

	Integer64 ts = 0;		// usec

	Gyro::State gyro;

	std::shared_ptr<GL_Tex2D> input_luma;

	ConstPointer output;		// GL_Tex2D
	ConstPointer output_rgb;	// GL_Tex2D

	ConstPointer gps_out;		// GPS::State
	ConstPointer loc_out;		// PositionWGS (localization)

	ConstPointer msp_rc;		// MSP2Client::RcPacket
	ConstPointer msp_alt;		// MSP2Client::Altitude

private:
	GPS gps_api;
	Gyro gyro_api;

	FlipImage flip_image;
	WeightRadius weight_radius;

	std::vector<Stage*> exec_chain;

	void init() override
	{
		flip_image.flip_x = src_flip_x;
		flip_image.flip_y = src_flip_y;

		input_luma = std::make_shared<GL_Tex2D>(width, height, GL_R8, GL_RED, GL_UNSIGNED_BYTE);

		flip_image.init(width, height, GL_R8, GL_RED, GL_UNSIGNED_BYTE);

		if(radius_mask > 0) {
			weight_radius.radius = (width / 2) * radius_mask;
		}
		weight_radius.init(GL_RED, width, height);

		add_output("ts", &ts);
		add_output("image", &output);
		add_output("image_rgb", &output_rgb);
		add_output("gyro", &gyro);
		add_output("gps", &gps_out);
		add_output("loc", &loc_out);
		add_output("msp_rc", &msp_rc);
		add_output("msp_alt", &msp_alt);

		for(auto stage : exec_chain) {
			stage->init();
		}

		if(do_record) {
			recorder = std::make_shared<Recorder>("record.dat");
		}
		have_init = true;
	}

	void init_wh(int w, int h)
	{
		width = w;
		height = h;

		init();
	}

	void exec() override
	{
		if(!have_init) {
			throw std::logic_error("!have_init");
		}
		if(!gyro_api.avail()) {
			std::cout << "WARN: Waiting for gyro init ..." << std::endl;
			return;
		}
		gyro = gyro_api.lookup(ts);

		loc_out = nullptr;

		if(gps_api.avail()) {
			try {
				if(auto gps = gps_api.lookup(ts, false))
				{
					if(gps->fix_type >= 2) {
						// provide GPS fallback
						auto pos = std::make_shared<PositionWGS>();
						pos->lat_deg = gps->lat_deg;
						pos->lon_deg = gps->lon_deg;
						pos->alt_m = gps->alt_m;
						loc_out = pos;
					}
					gps_out = gps;
				}
			} catch(...) {
				gps_out = nullptr;
				loc_out = nullptr;
			}
		}

		flip_image.exec(input_luma);

		weight_radius.exec(flip_image.out);

		output = weight_radius.out;

		for(auto stage : exec_chain) {
			try {
				stage->exec();
			} catch(const std::exception& ex) {
				throw std::runtime_error(stage->stage_name + ": " + ex.what());
			}
		}
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
				init_wh(w, h);
			}
			input_luma->upload(img_luma.data(), w);

//			show(display, img_rgba, w, h, 4);
		}
		else if(img->format == "YUV420") {
			if(!have_init) {
				init_wh(img->width, img->height);
			}
			input_luma->upload(img->data[0].data(), img->stride);
		}
		else {
			return;
		}

		{
			const int64_t ts_off = img->ts - img->timestamp;
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

		try {
			exec();
		} catch(const std::exception& ex) {
			std::cout << "ERROR: " << ex.what() << std::endl;
		}

		std::cout << "[" << img->sequence << "] total_time = " << (get_time_micros() - begin) / 1e3 << " ms" << std::endl;
	}

	void on_sample(std::shared_ptr<Sample> sample)
	{
		std::string topic = "null";

		if(auto img = std::dynamic_pointer_cast<Image>(sample)) {
			topic = "camera.nav";
			on_image(img);
			if(recorder) {
				sample = img->to_jpeg();
			}
		}
		else if(auto imu = std::dynamic_pointer_cast<MSP2::RawImu>(sample)) {
			topic = "msp.raw_imu";
			gyro_api.on_raw_imu(*imu);
		}
		else if(auto att = std::dynamic_pointer_cast<MSP2::Attitude>(sample)) {
			topic = "msp.attitude";
			gyro_api.on_attitude(*att);
			std::cout << "ATT: ts = " << att->ts << ", roll = " << att->roll << ", pitch = " << att->pitch << ", yaw = " << att->yaw << std::endl;
		}
		else if(auto alt = std::dynamic_pointer_cast<MSP2::Altitude>(sample)) {
			topic = "msp.altitude";
			msp_alt = alt;
			std::cout << "ALT: ts = " << alt->ts << ", alt_cm = " << alt->alt_cm << ", vario_cms = " << alt->vario_cms << std::endl;
		}
		else if(auto rc = std::dynamic_pointer_cast<MSP2::RcPacket>(sample)) {
			topic = "msp.rc";
			msp_rc = rc;
			std::cout << "RC: ts = " << rc->ts << ", roll = " << rc->roll() << ", pitch = " << rc->pitch() << ", yaw = " << rc->yaw() << ", throttle = " << rc->throttle() << std::endl;
		}
		else if(auto gps = std::dynamic_pointer_cast<MSP2::RawGPS>(sample)) {
			topic = "msp.raw_gps";
			gps_api.on_gps(*gps);
			std::cout << "GPS: lat = " << gps->lat << ", lon = " << gps->lon
					<< ", speed = " << gps->speed << ", heading = " << gps->course
					<< ", alt = " << gps->alt << ", sats = " << int(gps->num_sats) << ", fix = " << int(gps->fix_type) << std::endl;
		}

		if(recorder && sample) {
			write_sample(*recorder, topic, *sample);
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

	std::shared_ptr<Recorder> recorder;

	bool have_init = false;
	bool time_init = false;

};






#endif /* INCLUDE_MMPILOT_PIPELINE_H_ */
