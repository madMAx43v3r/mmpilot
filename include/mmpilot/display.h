/*
 * display.h
 *
 *  Created on: Feb 11, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_DISPLAY_H_
#define INCLUDE_MMPILOT_DISPLAY_H_

#include <mmpilot/egl.h>
#include <mmpilot/opengl.h>
#include <mmpilot/texture.h>
#include <mmpilot/util.h>

#include <array>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <memory>
#include <iostream>
#include <functional>


namespace mmpilot {

class TexDisplay {
public:
	TexDisplay(int width, int height)
		:	thread(std::bind(&TexDisplay::main, this, width, height))
	{
	}

	TexDisplay(const TexDisplay&) = delete;
	TexDisplay& operator=(const TexDisplay&) = delete;

	~TexDisplay() {
		close();
	}

	void show(const std::vector<uint8_t>& img, const int w, const int h, const int N)
	{
		std::lock_guard<std::mutex> lock(mutex);

		if(img.size() != w * h * N) {
			throw std::logic_error("TexDisplay::show(): dimension mismatch");
		}
		buf_width = w;
		buf_height = h;
		buffer.resize(w * h * 4);
		do_update = true;

		if(N == 4) {
			buffer = img;
		}
		else if(N == 1) {
			for(size_t i = 0; i < buffer.size(); ++i) {
				buffer[i] = (i % 4) == 3 ? 255 : img[i / 4];
			}
		}
		else if(N == 2) {
			for(size_t i = 0; i < buffer.size(); ++i) {
				const auto k = i % 4;
				if(k < 2) {
					buffer[i] = img[(i / 4) * 2 + k];
				} else if(k == 2) {
					buffer[i] = 0;
				} else {
					buffer[i] = 255;
				}
			}
		}
		else if(N == 3) {
			for(size_t i = 0; i < buffer.size(); ++i) {
				const auto k = i % 4;
				if(k < 3) {
					buffer[i] = img[(i / 4) * 3 + k];
				} else {
					buffer[i] = 255;
				}
			}
		}
	}

	void show(const std::vector<float>& img, const int w, const int h, const int N, const std::array<float, 4>& scale)
	{
		if(img.size() != w * h * N) {
			throw std::logic_error("TexDisplay::show(): dimension mismatch");
		}
		std::vector<uint8_t> tmp(img.size());

		const int mask = N - 1;

		for(size_t i = 0; i < img.size(); ++i)
		{
			auto v = img[i] * scale[i & mask] * 255.f;
			if(v < 0) v = 0;
			if(v > 255) v = 255;
			tmp[i] = v;
		}
		show(tmp, w, h, N);
	}

	void show(std::shared_ptr<GL_Tex2D> tex, const std::array<float, 4>& scale = {1, 1, 1, 1})
	{
		int N = 1;
		switch(tex->format) {
			case GL_RG:		N = 2; break;
			case GL_RGB:	N = 3; break;
			case GL_RGBA:	N = 4; break;
		}
		switch(tex->type) {
			case GL_UNSIGNED_BYTE:
				show(tex->download_u8(), tex->width, tex->height, N);
				break;
			case GL_FLOAT:
			case GL_HALF_FLOAT:
				show(tex->download_f32(), tex->width, tex->height, N, scale);
				break;
			default:
				throw std::logic_error("unsupported texture type");
		}
	}

	void close()
	{
		do_run = false;
		if(thread.joinable()) {
			thread.join();
		}
	}

private:
	std::thread thread;

	std::mutex mutex;
	int buf_width = 0;
	int buf_height = 0;
	std::vector<uint8_t> buffer;

	std::atomic_bool do_run {true};
	std::atomic_bool do_update {false};

	void main(int width, int height);

};


inline void show(std::unique_ptr<TexDisplay>& display, std::shared_ptr<GL_Tex2D> tex, const std::array<float, 4>& scale = {1, 1, 1, 1})
{
	if(!display) {
		display = std::make_unique<TexDisplay>(tex->width, tex->height);
	}
	display->show(tex, scale);
}

inline void show(std::unique_ptr<TexDisplay>& display, const std::vector<uint8_t>& img, int w, int h, int N)
{
	if(!display) {
		display = std::make_unique<TexDisplay>(w, h);
	}
	display->show(img, w, h, N);
}


} // mmpilot

#endif /* INCLUDE_MMPILOT_DISPLAY_H_ */
