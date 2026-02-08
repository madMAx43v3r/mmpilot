/*
 * helpers.h
 *
 *  Created on: Feb 5, 2026
 *      Author: mad
 */

#ifndef INCLUDE_HELPERS_H_
#define INCLUDE_HELPERS_H_

#include <string>
#include <stdexcept>


namespace mmpilot {

inline void die(const char* msg) {
	throw std::runtime_error(msg);
}

void wait_for_exit();

std::string read_file_txt(const std::string& path);



} // mmpilot

#endif /* INCLUDE_HELPERS_H_ */
