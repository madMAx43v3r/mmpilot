/*
 * camera.cpp
 *
 *  Created on: Feb 7, 2026
 *      Author: mad
 */

#include <mmpilot/camera.h>

#include <libcamera/libcamera.h>

#include <chrono>
#include <cstring>
#include <exception>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <sys/mman.h>
#include <thread>
#include <vector>

using namespace libcamera;


struct MappedPlane {
    void *addr = nullptr;
    size_t length = 0;
};

static std::vector<MappedPlane> mmapFrameBuffer(const FrameBuffer &buffer)
{
    std::vector<MappedPlane> planes;
    planes.reserve(buffer.planes().size());

    for (const FrameBuffer::Plane &p : buffer.planes()) {
        if (p.fd.get() < 0)
            throw std::runtime_error("Plane has invalid fd");

        void *addr = mmap(nullptr, p.length, PROT_READ | PROT_WRITE, MAP_SHARED, p.fd.get(), p.offset);
        if (addr == MAP_FAILED)
            throw std::runtime_error("mmap() failed");

        planes.push_back({addr, p.length});
    }
    return planes;
}

static void munmapFrameBuffer(const std::vector<MappedPlane> &planes)
{
    for (auto &pl : planes) {
        if (pl.addr && pl.length)
            munmap(pl.addr, pl.length);
    }
}

static bool isRaw10Bayer(const PixelFormat &pf)
{
    // libcamera PixelFormat::toString() returns names like "SRGGB10", "SGBRG10", "SBGGR10", "SGRBG10"
    std::string s = pf.toString();
    if (s.size() < 6) return false;
    if (s.find("10") == std::string::npos) return false;
    if (s.rfind("S", 0) != 0) return false; // starts with 'S'
    return (s.find("RGGB") != std::string::npos) ||
           (s.find("BGGR") != std::string::npos) ||
           (s.find("GBRG") != std::string::npos) ||
           (s.find("GRBG") != std::string::npos);
}

static std::optional<PixelFormat> pickRaw10Format(Camera *cam)
{
    // Ask the pipeline what RAW role formats are supported and pick a RAW10 Bayer one.
    std::unique_ptr<CameraConfiguration> cfg = cam->generateConfiguration({StreamRole::Raw});
    if (!cfg || cfg->empty())
        return std::nullopt;

    StreamConfiguration &sc = cfg->at(0);
    const StreamFormats &fmts = sc.formats();

    for (const PixelFormat &pf : fmts.pixelformats()) {
        if (isRaw10Bayer(pf))
            return pf;
    }
    return std::nullopt;
}

void Camera::run()
{
    std::signal(SIGINT, onSigInt);

    try {
        auto cm = std::make_unique<CameraManager>();
        if (cm->start())
            throw std::runtime_error("Failed to start CameraManager");

        if (cm->cameras().empty())
            throw std::runtime_error("No cameras found");

        std::shared_ptr<Camera> cam = cm->cameras()[0];
        std::cout << "Using camera: " << cam->id() << "\n";

        if (cam->acquire())
            throw std::runtime_error("Failed to acquire camera");

        // Choose RAW stream config
        std::unique_ptr<CameraConfiguration> config =
            cam->generateConfiguration({StreamRole::Raw});
        if (!config || config->empty())
            throw std::runtime_error("Failed to generate RAW configuration");

        StreamConfiguration &sc = config->at(0);

        auto raw10 = pickRaw10Format(cam.get());
        if (!raw10)
            throw std::runtime_error("No RAW10 Bayer format found for this camera/pipeline");

        sc.pixelFormat = *raw10;

        // Set desired size. Pipelines may adjust/crop to a supported RAW mode.
        sc.size.width  = 2304;  // example; choose what you want
        sc.size.height = 1296;

        CameraConfiguration::Status status = config->validate();
        if (status == CameraConfiguration::Invalid)
            throw std::runtime_error("RAW configuration invalid");

        // After validate(), sc may be modified to a supported mode.
        std::cout << "Configured RAW: "
                  << sc.pixelFormat.toString() << " "
                  << sc.size.width << "x" << sc.size.height
                  << " stride=" << sc.stride
                  << "\n";

        if (cam->configure(config.get()))
            throw std::runtime_error("Failed to configure camera");

        Stream *stream = sc.stream();

        // Allocate buffers
        FrameBufferAllocator allocator(cam);
        if (allocator.allocate(stream) < 0)
            throw std::runtime_error("Failed to allocate buffers");

        const auto &buffers = allocator.buffers(stream);
        if (buffers.empty())
            throw std::runtime_error("No buffers allocated");

        // Map buffers once
        std::map<FrameBuffer *, std::vector<MappedPlane>> mappings;
        for (auto &buf : buffers)
            mappings[buf.get()] = mmapFrameBuffer(*buf);

        // Create one request per buffer
        std::vector<std::unique_ptr<Request>> requests;
        requests.reserve(buffers.size());
        for (auto &buf : buffers) {
            std::unique_ptr<Request> req = cam->createRequest();
            if (!req)
                throw std::runtime_error("Failed to create request");

            if (req->addBuffer(stream, buf.get()) < 0)
                throw std::runtime_error("Failed to add buffer to request");

            requests.push_back(std::move(req));
        }

        // Optional: dump each frame to a file (turn off for speed)
        const bool dumpToFiles = false;
        std::atomic<uint64_t> frameCount{0};

        cam->requestCompleted.connect([&](Request *request) {
            if (request->status() == Request::RequestCancelled)
                return;

            auto it = request->buffers().find(stream);
            if (it == request->buffers().end())
                return;

            FrameBuffer *fb = it->second;
            const FrameMetadata &meta = fb->metadata();

            uint64_t n = ++frameCount;
            std::cout << "Frame " << n
                      << " seq=" << meta.sequence
                      << " ts=" << meta.timestamp
                      << " bytesused0=" << (meta.planes().empty() ? 0 : meta.planes()[0].bytesused)
                      << "\n";

            // Access RAW plane (usually 1 plane for packed RAW10)
            auto mit = mappings.find(fb);
            if (mit != mappings.end() && !mit->second.empty()) {
                const uint8_t *data = static_cast<const uint8_t *>(mit->second[0].addr);

                // RAW10 is typically packed: 4 pixels = 5 bytes (MIPI RAW10 packing).
                // NOTE: stride matters; total bytes can be >= width*height*10/8.
                if (dumpToFiles) {
                    std::string fn = "frame_" + std::to_string(n) + "_" +
                                     std::to_string(sc.size.width) + "x" +
                                     std::to_string(sc.size.height) + "_" +
                                     sc.pixelFormat.toString() + ".raw";
                    std::ofstream out(fn, std::ios::binary);
                    out.write(reinterpret_cast<const char *>(data), mit->second[0].length);
                }
            }

            // Re-queue for endless streaming
            request->reuse(Request::ReuseBuffers);
            if (cam->queueRequest(request) < 0)
                do_run = false;
        });

        // Start camera
        if (cam->start())
            throw std::runtime_error("Failed to start camera");

        // Queue initial requests
        for (auto &req : requests) {
            if (cam->queueRequest(req.get()) < 0)
                throw std::runtime_error("Failed to queue request");
        }

        std::cout << "Capturing RAW10 forever... Ctrl+C to stop\n";
        while (do_run)
            std::this_thread::sleep_for(std::chrono::milliseconds(50));

        std::cout << "Stopping...\n";
        cam->stop();

        for (auto &kv : mappings)
            munmapFrameBuffer(kv.second);

        allocator.free(stream);
        cam->release();
        cm->stop();
        return 0;

    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}



