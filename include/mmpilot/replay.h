/*
 * replay.h
 *
 *  Created on: Feb 8, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_REPLAY_H_
#define INCLUDE_MMPILOT_REPLAY_H_

#include <mmpilot/threads.h>
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

	// [topic, callback]
	std::map<std::string, std::function<std::shared_ptr<Sample>(Player&)>> decode;

	// [topic, callback]
	std::map<std::string, std::function<void(std::shared_ptr<Sample>)>> handle;

	Player(const std::string& file_name, size_t num_threads = 4)
		:	stream(file_name, std::ios::binary | std::ios::in),
			threads(num_threads)
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

	int32_t read_i32() {
		return read_pod<int32_t>();
	}

	int64_t read_i64() {
		return read_pod<int64_t>();
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

	bool read_sample()
	{
		const auto magic = read_u32();
		if(magic == eof_magic) {
			return false;
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

		auto mutex = topic_lock[topic];
		if(!mutex) {
			mutex = (topic_lock[topic] = std::make_shared<std::mutex>());
		}

		const auto f_decode = decode[topic];
		const auto f_handle = handle[topic];

		if(!f_decode) {
			throw std::runtime_error("missing decoder for " + topic);
		}
		const auto sample = f_decode(*this);
		if(!sample) {
			throw std::runtime_error("decode failed");
		}
		sample->topic = topic;

		const auto now_us = get_time_micros();

		if(have_init) {
			const int64_t delay_us = (ts + ts_delta) - now_us;
			if(real_time && delay_us > 0) {
				sleep_us(delay_us);		// sleep to match real time
			}
		} else {
			ts_delta = now_us - ts;		// initialize time delta
			have_init = true;
		}

		if(f_handle) {
			threads.dispatch([this, mutex, sample, f_handle]() {
				// make sure topics are not processed in parallel
				std::lock_guard<std::mutex> lock(*mutex);
				f_handle(sample);
			});
		}
		return true;
	}

	void play()
	{
		while(true) {
			try {
				if(!read_sample()) {
					std::cerr << "Player: EOF" << std::endl;
					break;
				}
			} catch(std::exception& ex) {
				std::cerr << "Player: " << ex.what() << std::endl;
				break;
			} catch(...) {
				break;
			}
		}
		threads.wait();
	}

private:
	std::ifstream stream;
	ThreadPool threads;

	int64_t ts_delta = 0;
	bool have_init = false;
	std::map<std::string, std::shared_ptr<std::mutex>> topic_lock;

	template<class T>
	T read_pod()
	{
		static_assert(std::is_integral<T>::value, "integral types only");
		T out = 0;
		read(&out, sizeof(T));
		return out;
	}

	static constexpr uint32_t eof_magic = 0x90ce9e5b;

};


} // mmpilot

#endif /* INCLUDE_MMPILOT_REPLAY_H_ */
