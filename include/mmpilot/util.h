/*
 * util.h
 *
 *  Created on: Feb 5, 2026
 *      Author: mad
 */

#ifndef INCLUDE_HELPERS_H_
#define INCLUDE_HELPERS_H_

#include <string>
#include <chrono>
#include <thread>
#include <stdexcept>


namespace mmpilot {

inline void die(std::string&& msg) {
	throw std::runtime_error(msg);
}

inline void sleep_us(size_t us) {
	std::this_thread::sleep_for(std::chrono::microseconds(us));
}

void wait_for_exit();

inline int64_t get_time_seconds() {
	return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

inline int64_t get_time_millis() {
	return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

inline int64_t get_time_micros() {
	return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

inline int64_t get_time_nanos() {
	return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

std::string read_file_txt(const std::string& path);



} // mmpilot

#endif /* INCLUDE_HELPERS_H_ */
