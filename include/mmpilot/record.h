/*
 * record.h
 *
 *  Created on: Feb 8, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_RECORD_H_
#define INCLUDE_MMPILOT_RECORD_H_

#include <string>
#include <cstdint>
#include <fstream>
#include <stdexcept>


namespace mmpilot {

class Recorder {
public:
	Recorder(const std::string& file_name)
		:	stream(file_name, std::ios::binary | std::ios::out | std::ios::trunc)
	{
		if(!stream.is_open()) {
			throw std::runtime_error("Recorder: failed to open file for writing");
		}
		stream.exceptions(std::ios::badbit); // throw on hard I/O errors
	}

	Recorder(const Recorder&) = delete;
	Recorder& operator=(const Recorder&) = delete;

	~Recorder() {
		try {
			close();
		} catch(...) {}
	}

	void write(const std::string& v) {
		write_u32(v.size());
		write_data(v.data(), v.size());
	}

	void write_i32(const int32_t& v) {
		write_pod(v);
	}

	void write_i64(const int64_t& v) {
		write_pod(v);
	}

	void write_u32(const uint32_t& v) {
		write_pod(v);
	}

	void write_u64(const uint64_t& v) {
		write_pod(v);
	}

	void write(const void* data, const size_t count)
	{
		ensure_open();
		write_u64(count);
		write_data(data, count);
	}

	void flush()
	{
		ensure_open();
		stream.flush();
		if(!stream) {
			throw std::runtime_error("Recorder: flush failed");
		}
	}

	void close()
	{
		if(stream.is_open()) {
			write_u32(eof_magic);
			flush();
			stream.close();
			if(stream.fail()) {
				throw std::runtime_error("Recorder: close failed");
			}
		}
	}

private:
	std::ofstream stream;

	void ensure_open() const {
		if(!stream.is_open()) {
			throw std::runtime_error("Recorder: stream is closed");
		}
	}

	template<class T>
	void write_pod(const T& v)
	{
		static_assert(std::is_integral<T>::value, "integral types only");
		ensure_open();
		write_data(&v, sizeof(T));
	}

	void write_data(const void* data, const size_t count)
	{
		stream.write(static_cast<const char*>(data), count);
		if(!stream) {
			throw std::runtime_error("Recorder: write failed");
		}
	}

	static constexpr uint32_t eof_magic = 0x90ce9e5b;

};


} // mmpilot

#endif /* INCLUDE_MMPILOT_RECORD_H_ */
