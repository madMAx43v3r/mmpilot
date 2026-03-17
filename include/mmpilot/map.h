/*
 * map.h
 *
 *  Created on: Mar 9, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_MAP_H_
#define INCLUDE_MMPILOT_MAP_H_

#include <mmpilot/record.h>
#include <mmpilot/replay.h>
#include <mmpilot/texture.h>
#include <mmpilot/wgs84.h>
#include <mmpilot/math.h>

#include <vector>


namespace mmpilot {

class Map {
public:
	size_t size = 0;			// width / height
	size_t format = 0;			// 1 = mono, 3 = rgb

	double lat0 = 0;			// center [rad]
	double lon0 = 0;			// center [rad]
	double alt0 = 0;			// center [m]
	double scale = 1;			// [m/px]

	std::vector<uint8_t> data;


	Vec2f gps_to_px(double lat_deg, double lon_deg) const
	{
		WGS84<double> origin(lat0, lon0, alt0);

		const Vec2d EN = origin.get_en(deg2rad(lat_deg), deg2rad(lon_deg)) / scale;
		return EN.cast<float>();
	}

	// includes weight channel
	std::shared_ptr<GL_Tex2D> upload_ex() const
	{
		GLenum gl_format = 0;
		GLenum int_format = 0;
		std::vector<uint8_t> tmp;
		switch(format) {
			case 1:
				gl_format = GL_RG;
				int_format = GL_RG8;
				tmp.resize(size * size * 2);
				for(size_t i = 0; i < size * size; ++i) {
					tmp[i * 2 + 0] = data[i];
					tmp[i * 2 + 1] = data[i] ? 255 : 0;
				}
				break;
			case 3:
				gl_format = GL_RGBA;
				int_format = GL_RGBA8;
				tmp.resize(size * size * 4);
				for(size_t i = 0; i < size * size; ++i) {
					tmp[i * 4 + 0] = data[i * 3 + 0];
					tmp[i * 4 + 1] = data[i * 3 + 1];
					tmp[i * 4 + 2] = data[i * 3 + 2];
					tmp[i * 4 + 3] = data[i * 3 + 0] && data[i * 3 + 1] && data[i * 3 + 2] ? 255 : 0;
				}
				break;
			default:
				throw std::logic_error("invalid format");
		}
		return std::make_shared<GL_Tex2D>(size, size, int_format, gl_format, GL_UNSIGNED_BYTE, tmp.data());
	}

	std::shared_ptr<GL_Tex2D> upload() const
	{
		GLenum gl_format = 0;
		GLenum int_format = 0;
		switch(format) {
			case 1:
				gl_format = GL_RED;
				int_format = GL_R8;
				break;
			case 3:
				gl_format = GL_RGB;
				int_format = GL_RGB8;
				break;
			default:
				throw std::logic_error("invalid format");
		}
		return std::make_shared<GL_Tex2D>(size, size, int_format, gl_format, GL_UNSIGNED_BYTE, data.data());
	}

	void write(const std::string& file_name) const
	{
		Recorder out(file_name);
		write(out);
	}

	static std::shared_ptr<Map> read(const std::string& file_name)
	{
		Reader in(file_name);
		return std::dynamic_pointer_cast<Map>(read(in));
	}

	void write(Recorder& out) const
	{
		out.write_u32(MAGIC);
		out.write_u32(0);
		out.write_u32(size);
		out.write_u32(format);
		out.write_u64(lat0 * 1e9);
		out.write_u64(lon0 * 1e9);
		out.write_u64(alt0 * 1e3);
		out.write_u64(scale * 1e6);
		out.write(data.data(), data.size());
	}

	static std::shared_ptr<Map> read(Reader& in)
	{
		const auto magic = in.read_u32();
		if(magic != MAGIC) {
			throw std::runtime_error("Map: invalid magic");
		}
		const auto version = in.read_u32();
		if(version != 0) {
			throw std::logic_error("Map: invalid version");
		}
		auto out = std::make_shared<Map>();
		out->size = in.read_u32();
		out->format = in.read_u32();
		out->lat0 = in.read_u64() / 1e9;
		out->lon0 = in.read_u64() / 1e9;
		out->alt0 = in.read_u64() / 1e3;
		out->scale = in.read_u64() / 1e6;
		const auto count = in.read_binary_size();
		if(count != out->size * out->size * out->format) {
			throw std::logic_error("Map: invalid size");
		}
		out->data.resize(count);
		in.read(out->data.data(), count);
		return out;
	}

private:
	static constexpr uint32_t MAGIC = 0x7269ddd0;

};


} // mmpilot

#endif /* INCLUDE_MMPILOT_MAP_H_ */
