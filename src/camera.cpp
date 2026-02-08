/*
 * camera.cpp
 *
 *  Created on: Feb 7, 2026
 *      Author: mad
 */

#include <mmpilot/camera.h>

#include <chrono>
#include <exception>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <sys/mman.h>
#include <vector>

using libcamera::Stream;
using libcamera::Request;
using libcamera::FrameBuffer;
using libcamera::PixelFormat;
using libcamera::StreamFormats;
using libcamera::CameraConfiguration;


namespace mmpilot {

static std::vector<std::pair<void*, size_t>> mmapFrameBuffer(const FrameBuffer& buffer)
{
	std::vector<std::pair<void*, size_t>> planes;

	for(const auto& p : buffer.planes()) {
		if(p.fd.get() < 0) {
			throw std::runtime_error("plane has invalid fd");
		}
		void* addr = ::mmap(nullptr, p.length, PROT_READ | PROT_WRITE, MAP_SHARED, p.fd.get(), p.offset);
		if(addr == MAP_FAILED) {
			throw std::runtime_error("mmap() failed");
		}
		planes.emplace_back(addr, p.length);
	}
	return planes;
}

static void munmapFrameBuffer(std::vector<std::pair<void*, size_t>>& planes)
{
	for(auto& p : planes) {
		if(p.first && p.second) {
			::munmap(p.first, p.second);
			p.first = nullptr;
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

std::string Camera::get_id() const
{
	if(!cam) {
		throw std::logic_error("open() first");
	}
	return cam->id();
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
	config = cam->generateConfiguration({libcamera::StreamRole::Raw});
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

	stride = sc.stride;
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
	}
}

void Camera::handle(Request* req)
{
	if(req->status() == Request::RequestCancelled) {
		return;
	}
	std::lock_guard<std::mutex> lock(mutex);

	const auto& buffers = req->buffers();

	const auto it = buffers.find(stream);
	if(it == buffers.end()) {
		return;
	}
	auto* fb = it->second;
	const auto& meta = fb->metadata();

	Frame out;
	out.width = width;
	out.height = height;
	out.sequence = meta.sequence;
	out.timestamp = meta.timestamp;
	out.pixel_format = pixel_format;

	// Find mapped buffer
	auto mit = mappings.find(fb);
	if(mit != mappings.end() && !mit->second.empty()) {
		if(on_frame) {
			out.data = mit->second;
			on_frame(out);
		}
	}

	// Re-queue for endless streaming
	req->reuse(Request::ReuseBuffers);
	if(cam->queueRequest(req) < 0) {
		cam->stop();
	}
}

void Camera::stop()
{
	cam->stop();
	cam->release();

	for(auto& kv : mappings) {
		munmapFrameBuffer(kv.second);
	}
	mappings.clear();

	allocator->free(stream);
}



} // mmpilot

