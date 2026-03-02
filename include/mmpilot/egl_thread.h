/*
 * egl_thread.h
 *
 *  Created on: Mar 2, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_EGL_THREAD_H_
#define INCLUDE_MMPILOT_EGL_THREAD_H_

#include <mmpilot/thread.h>
#include <mmpilot/egl.h>
#include <mmpilot/render.h>


namespace mmpilot {

class EGL_Thread : public Thread {
public:
	EGL_Thread() = default;

	EGL_Thread(EGLContext parent) : parent_(parent) {}

	void start() {
		start(&EGL_Thread::gl_main_func);
	}

private:
	EGLContext parent_ = nullptr;

	static void gl_main_func(Thread& self)
	{
		auto egl = EGL_create_context(3, parent_);

		render::init();

		self.run();

		render::cleanup();

		egl.terminate();
	}

};


} // mmpilot

#endif /* INCLUDE_MMPILOT_EGL_THREAD_H_ */
