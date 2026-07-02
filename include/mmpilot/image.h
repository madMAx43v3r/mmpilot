/*
 * image.h
 *
 *  Created on: Feb 11, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_IMAGE_H_
#define INCLUDE_MMPILOT_IMAGE_H_

#include <mmpilot/sample.h>
#include <mmpilot/replay.h>
#include <mmpilot/jpeg.h>

#include <vector>


namespace mmpilot {

class Image : public Sample {
public:
	int width = 0;
	int height = 0;
	int stride = 0;
	int exposure = 0;			// [us]
	float analog_gain = 1;
	uint64_t sequence = 0;
	uint64_t timestamp = 0;		// [us]
	std::string format;
	std::vector<std::vector<uint8_t>> data;

	void write(Recorder& out) const override
	{
		out.write_u32(MAGIC);
		out.write_u32(0);
		out.write_u32(width);
		out.write_u32(height);
		out.write_u32(stride);
		out.write_u32(exposure);
		out.write_u32(analog_gain * 1000);
		out.write_u64(sequence);
		out.write_u64(timestamp);
		out.write(format);

		out.write_u32(data.size());
		for(const auto& v : data) {
			out.write(v.data(), v.size());
		}
	}

	static std::shared_ptr<Sample> read(Player& in)
	{
		const auto magic = in.read_u32();
		if(magic != MAGIC) {
			throw std::runtime_error("Image: invalid magic");
		}
		const auto version = in.read_u32();
		if(version != 0) {
			throw std::runtime_error("Image: invalid version");
		}
		auto out = std::make_shared<Image>();
		out->width = in.read_u32();
		out->height = in.read_u32();
		out->stride = in.read_u32();
		out->exposure = in.read_u32();
		out->analog_gain = in.read_u32() / 1000.f;
		out->sequence = in.read_u64();
		out->timestamp = in.read_u64();
		out->format = in.read_string();

		const auto N = in.read_u32();
		if(N > 64) {
			throw std::runtime_error("Image: N > 64");
		}
		for(uint32_t i = 0; i < N; ++i) {
			const auto size = in.read_binary_size();
			std::vector<uint8_t> tmp(size);
			in.read(tmp.data(), size);
			out->data.push_back(std::move(tmp));
		}
		return out;
	}

	std::shared_ptr<Image> to_jpeg(int quality = 95) const
	{
		if(format != "YUV420" || data.size() != 3) {
			return nullptr;
		}
		const auto* Y = data[0].data();
		const auto* U = data[1].data();
		const auto* V = data[2].data();

		auto out = std::make_shared<Image>();
		out->ts = ts;
		out->width = width;
		out->height = height;
		out->stride = width;
		out->exposure = exposure;
		out->analog_gain = analog_gain;
		out->sequence = sequence;
		out->timestamp = timestamp / 1000;
		out->format = "JPEG";
		out->data.push_back(encode_jpeg_i420(
				Y, U, V, width, height, stride, quality));
		return out;
	}

private:
	static constexpr uint32_t MAGIC = 0x7cfd4bfc;

};


} // mmpilot

#endif /* INCLUDE_MMPILOT_IMAGE_H_ */
