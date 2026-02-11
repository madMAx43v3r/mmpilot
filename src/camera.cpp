/*
 * camera.cpp
 *
 *  Created on: Feb 7, 2026
 *      Author: mad
 */

#include <mmpilot/camera.h>

#include <memory>
#include <string>
#include <vector>
#include <cstring>
#include <stdexcept>
#include <iostream>
#include <sys/mman.h>

using libcamera::Stream;
using libcamera::Request;
using libcamera::FrameBuffer;
using libcamera::PixelFormat;
using libcamera::StreamFormats;
using libcamera::CameraConfiguration;


namespace mmpilot {

std::shared_ptr<libcamera::CameraManager> Camera::g_manager;


inline size_t align_up(size_t x, size_t a) {
	return (x + (a - 1)) & ~(a - 1);
}

static std::vector<Camera::MappedPlane> mmapFrameBuffer(const FrameBuffer& buffer)
{
	const size_t PAGE_SIZE = 4096;

	// required bytes per fd = max(offset+len)
	std::unordered_map<int, size_t> need;

	for(const auto& p : buffer.planes())
	{
		const int fd = p.fd.get();
		if(fd < 0) {
			throw std::runtime_error("plane has invalid fd");
		}
		size_t end = p.offset + p.length;
		need[fd] = std::max(need[fd], end);
	}

	// mmap once per fd at offset 0
	struct MapInfo {
		void* base;
		size_t len;
	};
	std::unordered_map<int, MapInfo> maps;

	for(auto& [fd, bytes] : need)
	{
		size_t map_len = align_up(bytes, PAGE_SIZE);
		void* base = ::mmap(nullptr, map_len, PROT_READ, MAP_SHARED, fd, 0);
		if(base == MAP_FAILED) {
			int e = errno;
			throw std::runtime_error(
					"mmap() failed: fd=" + std::to_string(fd) + " offset=0 map_len=" + std::to_string(map_len)
							+ " errno=" + std::to_string(e) + " (" + std::strerror(e) + ")");
		}
		maps[fd] = {base, map_len};
	}

	// produce plane pointers
	std::vector<Camera::MappedPlane> out;

	for(const auto& p : buffer.planes())
	{
		const auto fd = p.fd.get();
		const auto mi = maps.at(fd);

		Camera::MappedPlane mp;
		mp.fd = fd;
		mp.base = mi.base;
		mp.base_length = mi.len;
		mp.addr = ((unsigned char*)mi.base) + p.offset;
		mp.length = (size_t)p.length;
		out.push_back(mp);
	}
	return out;
}

static void munmapFrameBuffer(std::vector<Camera::MappedPlane>& planes)
{
	for(auto& p : planes) {
		if(p.base && p.base_length) {
			::munmap(p.base, p.base_length);
			p.base = nullptr;
		}
	}
}

void Camera::init()
{
	g_manager = std::make_shared<libcamera::CameraManager>();
	if(g_manager->start()) {
		throw std::runtime_error("failed to start CameraManager");
	}
}

void Camera::cleanup()
{
	g_manager->stop();
	g_manager = nullptr;
}


Camera::Camera(int index, int stream, int width, int height, std::string pixel_format)
	:	camera_index(index), stream_index(stream), width(width), height(height), pixel_format(pixel_format)
{
}

std::string Camera::get_id() const
{
	if(!cam) {
		throw std::logic_error("open() first");
	}
	return cam->id();
}

void Camera::set_exposure(int exposure_ms)
{
	controls().set(libcamera::controls::ExposureTime, exposure_ms);
}

void Camera::set_interval(int interval_ms)
{
	const auto frame_us = int64_t(interval_ms) * 1000;
	controls().set(libcamera::controls::FrameDurationLimits,
				libcamera::Span<const int64_t, 2>({frame_us, frame_us}));
}

libcamera::ControlList& Camera::controls()
{
	if(!control) {
		throw std::logic_error("open() first");
	}
	return *control;
}

void Camera::open()
{
	if(!g_manager) {
		throw std::logic_error("init() first");
	}
	const auto& cameras = g_manager->cameras();
	if(cameras.empty()) {
		throw std::runtime_error("no cameras found");
	}
	if(camera_index >= cameras.size()) {
		throw std::runtime_error("camera index out of bounds");
	}
	cam = cameras.at(camera_index);
	std::cout << "Using camera: " << cam->id() << "\n";

	if(cam->acquire()) {
		throw std::runtime_error("failed to acquire camera");
	}
	config = cam->generateConfiguration({libcamera::StreamRole::VideoRecording});
	if(!config || config->empty()) {
		throw std::runtime_error("failed to generate RAW configuration");
	}
	auto& sc = config->at(stream_index);
	sc.pixelFormat = libcamera::PixelFormat::fromString(pixel_format);
	sc.size.width = width;
	sc.size.height = height;

	const auto status = config->validate();
	if(status == CameraConfiguration::Invalid) {
		throw std::runtime_error("RAW configuration invalid");
	}
	std::cout << "Configured RAW: " << sc.pixelFormat.toString()
			<< ", " << sc.size.width << "x" << sc.size.height
			<< ", stride = " << sc.stride << ", buffer_count = " << sc.bufferCount << std::endl;

	width = sc.size.width;
	height = sc.size.height;
	stride = sc.stride;
	pixel_format = sc.pixelFormat.toString();
	buffer_count = sc.bufferCount;

	if(cam->configure(config.get())) {
		throw std::runtime_error("failed to configure camera");
	}
	stream = sc.stream();
	control = std::make_unique<libcamera::ControlList>(cam->controls());
	allocator = std::make_unique<libcamera::FrameBufferAllocator>(cam);
}

void Camera::start()
{
	// Allocate buffers
	if(allocator->allocate(stream) < 0) {
		throw std::runtime_error("failed to allocate buffers");
	}
	const auto& buffers = allocator->buffers(stream);
	if(buffers.empty()) {
		throw std::runtime_error("no buffers allocated");
	}
	for(auto& buf : buffers) {
		mappings[buf.get()] = mmapFrameBuffer(*buf);
	}

	// Create one request per buffer
	requests.clear();
	requests.reserve(buffers.size());
	for(auto& buf : buffers) {
		std::unique_ptr<Request> req = cam->createRequest();
		if(!req) {
			throw std::runtime_error("failed to create request");
		}
		if(req->addBuffer(stream, buf.get()) < 0) {
			throw std::runtime_error("failed to add buffer to request");
		}
		requests.push_back(std::move(req));
	}

	cam->requestCompleted.connect(this, &Camera::handle);

	// Start camera
	if(cam->start(control.get())) {
		throw std::runtime_error("failed to start camera");
	}

	// Queue initial requests
	for(auto& req : requests) {
		if(cam->queueRequest(req.get()) < 0) {
			throw std::runtime_error("Failed to queue request");
		}
		num_pending++;
	}
}

void Camera::handle(Request* req)
{
	std::lock_guard<std::mutex> lock(mutex);

	num_pending--;
	signal.notify_all();

	if(!do_run || req->status() == Request::RequestCancelled) {
		return;
	}
	const auto& buffers = req->buffers();

	const auto it = buffers.find(stream);
	if(it == buffers.end()) {
		return;
	}
	auto* fb = it->second;
	const auto& fm = fb->metadata();
	const auto& meta = req->metadata();

	if(fm.sequence == 0) {
		const auto* id_map = meta.idMap();
		std::cout << "---- metadata (" << meta.size() << ") ----" << std::endl;
		for(const auto& it : meta) {
			auto iter_id = id_map->find(it.first);
			if(iter_id != id_map->end()) {
				const auto* id = iter_id->second;
				const auto& val = it.second;
				std::cout << id->name() << " = " << val.toString() << std::endl;
			}
		}
	}

	CameraFrame out;
	out.width = width;
	out.height = height;
	out.stride = stride;
	if(auto v = meta.get(libcamera::controls::ExposureTime)) {
		out.exposure = *v;
	}
	if(auto v = meta.get(libcamera::controls::AnalogueGain)) {
		out.analog_gain = *v;
	}
	out.sequence = fm.sequence;
	out.timestamp = fm.timestamp;
	out.pixel_format = pixel_format;

	// Find mapped buffer
	auto mit = mappings.find(fb);
	if(mit != mappings.end()) {
		if(on_frame) {
			for(const auto& entry : mit->second) {
				out.data.emplace_back(entry.addr, entry.length);
			}
			on_frame(out);
		}
	}

	// Re-queue for endless streaming
	req->reuse(Request::ReuseBuffers);
	if(cam->queueRequest(req) < 0) {
		do_run = false;
	} else {
		num_pending++;
	}
}

void Camera::stop()
{
	cam->stop();
	{
		std::unique_lock<std::mutex> lock(mutex);
		do_run = false;
		while(num_pending > 0) {
			signal.wait(lock);
		}
	}
	cam->release();

	for(auto& kv : mappings) {
		munmapFrameBuffer(kv.second);
	}
	mappings.clear();

	allocator->free(stream);
}



} // mmpilot

