/*
 * egl.h
 *
 *  Created on: Feb 10, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_EGL_H_
#define INCLUDE_MMPILOT_EGL_H_

#include <EGL/egl.h>

#include <string>


namespace mmpilot {

struct EglCtx {
	EGLContext ctx = EGL_NO_CONTEXT;
	EGLDisplay display = EGL_NO_DISPLAY;
	EGLSurface surface = EGL_NO_SURFACE;

	~EglCtx();
	void terminate();
};


EglCtx EGL_create_context(int gles_major = 3);

std::string EGL_error_name(EGLint e);

void EGL_check(const char* where);


} // mmpilot

#endif /* INCLUDE_MMPILOT_EGL_H_ */
