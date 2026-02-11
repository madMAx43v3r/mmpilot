/*
 * jpeg.cpp
 *
 *  Created on: Feb 11, 2026
 *      Author: mad
 */

#include <mmpilot/jpeg.h>

#include <turbojpeg.h>

#include <stdexcept>


namespace mmpilot {

std::vector<uint8_t> decode_jpeg_y(
		const uint8_t* jpg, const size_t jpg_size, int& width, int& height, const int flags)
{
	tjhandle ctx = tjInitDecompress();
	if(!ctx) {
		throw std::runtime_error("tjInitDecompress() failed");
	}

	int subsamp = 0, cs = 0;
	if(tjDecompressHeader3(ctx, jpg, jpg_size, &width, &height, &subsamp, &cs))
	{
		std::string err = tjGetErrorStr2(ctx);
		tjDestroy(ctx);
		throw std::runtime_error("tjDecompressHeader3() failed: " + err);
	}
	std::vector<uint8_t> rgba(width * height);

	if(tjDecompress2(ctx, jpg, jpg_size, rgba.data(), width, 0, height, TJPF_GRAY, flags))
	{
		std::string err = tjGetErrorStr2(ctx);
		tjDestroy(ctx);
		throw std::runtime_error("tjDecompress2() failed: " + err);
	}
	tjDestroy(ctx);
	return rgba;
}

std::vector<uint8_t> decode_jpeg_rgba(
		const uint8_t* jpg, const size_t jpg_size, int& width, int& height, const int flags)
{
	tjhandle ctx = tjInitDecompress();
	if(!ctx) {
		throw std::runtime_error("tjInitDecompress() failed");
	}

	int subsamp = 0, cs = 0;
	if(tjDecompressHeader3(ctx, jpg, jpg_size, &width, &height, &subsamp, &cs))
	{
		std::string err = tjGetErrorStr2(ctx);
		tjDestroy(ctx);
		throw std::runtime_error("tjDecompressHeader3() failed: " + err);
	}
	std::vector<uint8_t> rgba(width * height * 4);

	if(tjDecompress2(ctx, jpg, jpg_size, rgba.data(), width, 0, height, TJPF_RGBA, flags))
	{
		std::string err = tjGetErrorStr2(ctx);
		tjDestroy(ctx);
		throw std::runtime_error("tjDecompress2() failed: " + err);
	}
	tjDestroy(ctx);
	return rgba;
}

// quality: 1..100
// flags: e.g. TJFLAG_FASTDCT
std::vector<uint8_t> encode_jpeg_i420(
		const void* Y, const void* U, const void* V,
		const int width, const int height, const int stride, const int quality, const int flags)
{
	if(width % 2 || height % 2) {
		throw std::logic_error("encode_jpeg_i420(): invalid dimensions");
	}
	tjhandle ctx = tjInitCompress();
	if(!ctx) {
		throw std::runtime_error("tjInitCompress failed");
	}

	const unsigned char* planes[3] = {(const unsigned char*)Y, (const unsigned char*)U, (const unsigned char*)V};
	const int strides[3] = {stride, stride / 2, stride / 2};

	// TurboJPEG will allocate output buffer unless you pass TJFLAG_NOREALLOC and provide one.
	unsigned char* jpeg = nullptr;
	unsigned long  jpeg_size = 0;

	const int rc = tjCompressFromYUVPlanes(
			ctx, planes, width, strides, height, TJSAMP_420, &jpeg, &jpeg_size, quality, flags);
	if(rc) {
		std::string err = tjGetErrorStr2(ctx);
		tjDestroy(ctx);
		throw std::runtime_error("tjCompressFromYUVPlanes() failed: " + err);
	}
	std::vector<uint8_t> out(jpeg, jpeg + jpeg_size);

	tjFree(jpeg);
	tjDestroy(ctx);
	return out;
}


} // mmpilot
