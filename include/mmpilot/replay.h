/*
 * replay.h
 *
 *  Created on: Feb 8, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_REPLAY_H_
#define INCLUDE_MMPILOT_REPLAY_H_

#include <mmpilot/sample.h>
#include <mmpilot/util.h>

#include <map>
#include <string>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <iostream>


namespace mmpilot {

class Player {
public:
	size_t max_string_len = 1u << 24;
	size_t max_binary_len = 1u << 28;

	bool real_time = true;

	double speed = 1;

	// [topic, callback]
	std::map<std::string, std::function<std::shared_ptr<Sample>(Player&)>> decode;

	// [topic, callback]
	std::map<std::string, std::function<void(std::shared_ptr<Sample>)>> handle;

	Player(const std::string& file_name)
		:	stream(file_name, std::ios::binary | std::ios::in)
	{
		if(!stream.is_open()) {
			throw std::runtime_error("Player: failed to open file for reading");
		}
		stream.exceptions(std::ios::badbit); // throw on hard I/O errors
	}

	Player(const Player&) = delete;
	Player& operator=(const Player&) = delete;

	~Player() {
		stream.close();
	}

	std::string read_string()
	{
		const auto N = read_u32();
		if(N > max_string_len) {
			throw std::runtime_error("string > max_string_len");
		}
		std::vector<char> tmp(N);
		read(tmp.data(), tmp.size());
		return std::string(tmp.data(), tmp.size());
	}

	int16_t read_i16() {
		return read_pod<int16_t>();
	}
	int32_t read_i32() {
		return read_pod<int32_t>();
	}
	int64_t read_i64() {
		return read_pod<int64_t>();
	}

	uint16_t read_u16() {
		return read_pod<uint16_t>();
	}
	uint32_t read_u32() {
		return read_pod<uint32_t>();
	}
	uint64_t read_u64() {
		return read_pod<uint64_t>();
	}

	uint64_t read_binary_size() {
		const auto size = read_u64();
		if(size > max_binary_len) {
			throw std::runtime_error("binary > max_binary_len");
		}
		return size;
	}

	void read(void* data, const size_t count)
	{
		stream.read(static_cast<char*>(data), count);
		if(!stream) {
			throw std::runtime_error("read failed or EOF reached");
		}
	}

	std::shared_ptr<Sample> read_sample()
	{
		const auto magic = read_u32();
		if(magic == eof_magic) {
			return nullptr;
		}
		if(magic != 0x3d171f57) {
			throw std::runtime_error("bad sample magic: " + std::to_string(magic));
		}
		const auto version = read_u32();
		if(version > 0) {
			throw std::runtime_error("invalid sample version");
		}
		const auto ts = read_i64();
		const auto topic = read_string();

		const auto f_decode = decode[topic];
		if(!f_decode) {
			throw std::runtime_error("missing decoder for " + topic);
		}
		const auto sample = f_decode(*this);
		if(!sample) {
			throw std::runtime_error("decode failed");
		}
		sample->ts = ts;
		sample->topic = topic;
		return sample;
	}

	void play()
	{
		bool have_init = false;
		int64_t ts_delta = 0;
		while(true) {
			try {
				if(auto sample = read_sample()) {
					const auto now_us = get_time_micros();
					if(have_init) {
						const int64_t delay_us = (sample->ts + ts_delta) / speed - now_us;
						if(real_time && delay_us > 0) {
							sleep_us(delay_us);		// sleep to match real time
						}
					} else {
						ts_delta = now_us - sample->ts;		// initialize time delta
						have_init = true;
					}
					dispatch(sample);
				} else {
					break;
				}
			} catch(std::exception& ex) {
				std::cerr << "Player: " << ex.what() << std::endl;
				break;
			} catch(...) {
				break;
			}
		}
	}

	void seek(const int64_t offset_ms)
	{
		if(offset_ms <= 0) {
			return;
		}
		int64_t base_ts = -1;
		while(auto sample = read_sample())
		{
			if(base_ts < 0) {
				base_ts = sample->ts;
			}
			if((sample->ts - base_ts) / 1000 >= offset_ms) {
				dispatch(sample);
				break;
			}
		}
	}

private:
	std::ifstream stream;

	template<class T>
	T read_pod()
	{
		static_assert(std::is_integral<T>::value, "integral types only");
		T out = 0;
		read(&out, sizeof(T));
		return out;
	}

	void dispatch(std::shared_ptr<Sample> sample)
	{
		const auto f_handle = handle[sample->topic];
		if(f_handle) {
			f_handle(sample);
		}
	}

	static constexpr uint32_t eof_magic = 0x90ce9e5b;

};


} // mmpilot

#endif /* INCLUDE_MMPILOT_REPLAY_H_ */
