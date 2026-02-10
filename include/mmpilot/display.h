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

#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <memory>
#include <functional>


namespace mmpilot {

class TexDisplay {
public:
	TexDisplay(int width, int height)
		:	thread(std::bind(&TexDisplay::main, this, width, height))
	{
	}

	~TexDisplay() {
		close();
	}

	void show(std::shared_ptr<GL_Tex2D> tex)
	{
		auto fbo = GL_create_FBO(tex->id);
		{
			std::lock_guard<std::mutex> lock(mutex);
			GL_read_FBO_RGBA(fbo, 0, tex->width, tex->height, buffer);
			do_update = true;
		}
		glDeleteFramebuffers(1, &fbo);
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
	std::vector<uint8_t> buffer;

	std::atomic_bool do_run {true};
	std::atomic_bool do_update {false};

	void main(int width, int height);

};


} // mmpilot

#endif /* INCLUDE_MMPILOT_DISPLAY_H_ */
