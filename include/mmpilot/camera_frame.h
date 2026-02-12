/*
 * camera_frame.h
 *
 *  Created on: Feb 9, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_CAMERA_FRAME_H_
#define INCLUDE_MMPILOT_CAMERA_FRAME_H_

#include <mmpilot/record.h>
#include <mmpilot/replay.h>
#include <mmpilot/sample.h>

#include <vector>
#include <string>
#include <stdexcept>


namespace mmpilot {

class CameraFrame : public Sample {
public:
	int width = 0;
	int height = 0;
	int stride = 0;				// bytes
	int exposure = 0;			// [us]
	float analog_gain = 1;
	uint64_t sequence = 0;
	uint64_t timestamp = 0;		// [ns]
	std::string pixel_format;
	std::vector<std::pair<void*, size_t>> data;

	CameraFrame(CameraFrame&) = delete;
	CameraFrame& operator=(CameraFrame&) = delete;

	~CameraFrame()
	{
		if(is_owner) {
			for(const auto& buf : data) {
				::free(buf.first);
			}
		}
	}

	void write(Recorder& out) const
	{
		out.write_u32(0xc08a5cba);
		out.write_u32(0);
		out.write_u32(width);
		out.write_u32(height);
		out.write_u32(stride);
		out.write_u32(exposure);
		out.write_u32(analog_gain * 1000);
		out.write_u64(sequence);
		out.write_u64(timestamp);
		out.write(pixel_format);

		out.write_u32(data.size());
		for(const auto& buf : data) {
			out.write(buf.first, buf.second);
		}
	}

	static std::shared_ptr<CameraFrame> read(Player& in)
	{
		const auto magic = in.read_u32();
		if(magic != 0xc08a5cba) {
			throw std::runtime_error("expected Camera::Frame");
		}
		const auto version = in.read_u32();
		if(version > 0) {
			throw std::runtime_error("invalid Camera::Frame version");
		}
		auto out = std::make_shared<CameraFrame>();
		out->width = in.read_u32();
		out->height = in.read_u32();
		out->stride = in.read_u32();
		out->exposure = in.read_u32();
		out->analog_gain = in.read_u32() / 1000.f;
		out->sequence = in.read_u64();
		out->timestamp = in.read_u64();
		out->pixel_format = in.read_string();

		const auto num_planes = in.read_u32();
		if(num_planes > 64) {
			throw std::logic_error("num_planes out of bounds");
		}
		for(uint32_t i = 0; i < num_planes; ++i) {
			const auto size = in.read_binary_size();
			const auto data = ::malloc(size);
			in.read(data, size);
			out->data.emplace_back(data, size);
		}
		out->is_owner = true;
		return out;
	}

private:
	bool is_owner = false;

};


} // mmpilot

#endif /* INCLUDE_MMPILOT_CAMERA_FRAME_H_ */
