/*
 * egl.h
 *
 *  Created on: Feb 10, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_EGL_H_
#define INCLUDE_MMPILOT_EGL_H_

#include <string>


typedef void* EGLContext;
typedef void* EGLDisplay;
typedef void* EGLSurface;

namespace mmpilot {

struct EglCtx {
	EGLContext ctx = nullptr;
	EGLDisplay display = nullptr;
	EGLSurface surface = nullptr;

	EglCtx();

	~EglCtx();

	void terminate();
};


EglCtx EGL_create_context(const int gles_major = 3, EGLContext parent = nullptr);

std::string EGL_error_name(int e);

void EGL_check(const char* where);


} // mmpilot

#endif /* INCLUDE_MMPILOT_EGL_H_ */
