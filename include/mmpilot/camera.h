/*
 * camera.h
 *
 *  Created on: Feb 7, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_CAMERA_H_
#define INCLUDE_MMPILOT_CAMERA_H_

#include <mmpilot/record.h>

#include <libcamera/libcamera.h>

#include <mutex>
#include <functional>
#include <condition_variable>


namespace mmpilot {

class Camera {
public:
	class Frame {
	public:
		int width = 0;
		int height = 0;
		int stride = 0;				// bytes
		uint64_t sequence = 0;
		uint64_t timestamp = 0;		// [ns]
		std::string pixel_format;
		std::vector<std::pair<void*, size_t>> data;

		void write(Recorder& out) const;
		~Frame();
	private:
		bool is_owner = false;
	};

	std::function<void(const Frame&)> on_frame;

	Camera(int index, int stream, int width, int height, std::string pixel_format);

	void open();

	void start();

	void stop();

	std::string get_id() const;

	void set_exposure(int exposure_ms);

	void set_interval(int interval_ms);

	libcamera::ControlList& controls();

	static void init();

	static void cleanup();

private:
	void handle(libcamera::Request* req);

private:
	int camera_index = 0;
	int stream_index = 0;
	int width = 0;
	int height = 0;
	int stride = 0;
	int buffer_count = 0;

	std::string pixel_format;

	libcamera::Stream* stream = nullptr;
	std::shared_ptr<libcamera::Camera> cam;
	std::unique_ptr<libcamera::ControlList> control;
	std::unique_ptr<libcamera::CameraConfiguration> config;
	std::unique_ptr<libcamera::FrameBufferAllocator> allocator;

	std::mutex mutex;
	bool do_run = true;
	int num_pending = 0;
	std::condition_variable signal;
	std::vector<std::unique_ptr<libcamera::Request>> requests;
	std::map<libcamera::FrameBuffer*, std::vector<std::pair<void*, size_t>>> mappings;

	static std::shared_ptr<libcamera::CameraManager> g_manager;

};


} // mmpilot

#endif /* INCLUDE_MMPILOT_CAMERA_H_ */
