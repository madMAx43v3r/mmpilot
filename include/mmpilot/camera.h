/*
 * camera.h
 *
 *  Created on: Feb 7, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_CAMERA_H_
#define INCLUDE_MMPILOT_CAMERA_H_

#include <mmpilot/camera_frame.h>

#include <libcamera/libcamera.h>

#include <map>
#include <mutex>
#include <memory>
#include <functional>
#include <condition_variable>


namespace mmpilot {

class Camera {
public:
	bool show_meta = false;

	std::function<void(const CameraFrame&)> on_frame;

	Camera(int index, int stream, int width, int height, std::string pixel_format);

	Camera(const Camera&) = delete;
	Camera& operator=(const Camera&) = delete;

	void open();

	void start();

	void stop();

	std::string get_id() const;

	void set_exposure(int exposure_ms);

	void set_interval(int interval_ms);

	libcamera::ControlList& controls();

	static void init();

	static void cleanup();

	struct MappedPlane {
		void*  addr = nullptr;      // plane start = base + offset
		void*  base = nullptr;      // fd mapping base (for munmap)
		size_t length = 0;          // plane length
		size_t base_length = 0;     // fd mapping length (for munmap)
		int    fd = -1;
	};

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
	std::map<libcamera::FrameBuffer*, std::vector<MappedPlane>> mappings;

	static std::shared_ptr<libcamera::CameraManager> g_manager;

};


} // mmpilot

#endif /* INCLUDE_MMPILOT_CAMERA_H_ */
