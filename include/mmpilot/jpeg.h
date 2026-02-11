/*
 * jpeg.h
 *
 *  Created on: Feb 11, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_JPEG_H_
#define INCLUDE_MMPILOT_JPEG_H_

#include <turbojpeg.h>

#include <vector>
#include <cstdint>
#include <cstddef>


namespace mmpilot {

std::vector<uint8_t> decode_jpeg_y(
		const uint8_t* jpg, const size_t jpg_size,
		int& width, int& height, const int flags = 0);

std::vector<uint8_t> decode_jpeg_rgba(
		const uint8_t* jpg, const size_t jpg_size,
		int& width, int& height, const int flags = 0);

// quality: 1..100
// flags: e.g. TJFLAG_FASTDCT
std::vector<uint8_t> encode_jpeg_i420(
		const void* Y, const void* U, const void* V,
		const int width, const int height, const int stride,
		const int quality = 85, const int flags = 0);


} // mmpilot

#endif /* INCLUDE_MMPILOT_JPEG_H_ */
