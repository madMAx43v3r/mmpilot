/*
 * egl.cpp
 *
 *  Created on: Feb 10, 2026
 *      Author: mad
 */

#include <mmpilot/egl.h>
#include <mmpilot/util.h>

#include <EGL/eglext.h>

#include <vector>


namespace mmpilot {

EglCtx::~EglCtx() {
	terminate();
}

void EglCtx::terminate()
{
	if(display != EGL_NO_DISPLAY) {
		eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
		if(surface != EGL_NO_SURFACE) {
			eglDestroySurface(display, surface);
			surface = EGL_NO_SURFACE;
		}
		if(ctx != EGL_NO_CONTEXT) {
			eglDestroyContext(display, ctx);
			ctx = EGL_NO_CONTEXT;
		}
		eglTerminate(display);
		display = EGL_NO_DISPLAY;
	}
}


static EGLDisplay EGL_get_display()
{
	// Prefer surfaceless on Mesa (headless-safe)
	auto eglGetPlatformDisplayEXT =
			(PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");

	if(eglGetPlatformDisplayEXT) {
		// Try Mesa surfaceless first
		EGLDisplay dpy = eglGetPlatformDisplayEXT(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, nullptr);

		if(dpy != EGL_NO_DISPLAY) {
			return dpy;
		}
		// If you want, you can add GBM here later.
	}
	// Fallback: whatever default platform is (X11/Wayland). Can fail headless/SSH.
	return eglGetDisplay(EGL_DEFAULT_DISPLAY);
}

EglCtx EGL_create_context(int gles_major)
{
	const auto display = EGL_get_display();
	if(display == EGL_NO_DISPLAY) {
		EGL_check("eglGetDisplay failed");
	}

	if(!eglInitialize(display, nullptr, nullptr)) {
		EGL_check("eglInitialize failed");
	}

	if(!eglBindAPI(EGL_OPENGL_ES_API)) {
		EGL_check("eglBindAPI(EGL_OPENGL_ES_API) failed");
	}

	// Choose a config that is compatible with ES3 context creation.
	// For surfaceless, EGL_SURFACE_TYPE can be 0; but many drivers expect PBUFFER_BIT,
	// so we request PBUFFER_BIT to keep config selection robust.
	const EGLint cfg_attribs[] = {
		EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
		EGL_NONE
	};

	EGLint num = 0;
	EGLConfig config = nullptr;
	if(!eglChooseConfig(display, cfg_attribs, &config, 1, &num) || num < 1) {
		EGL_check("eglChooseConfig failed (no suitable EGLConfig)");
	}

	// Try to create ES 3.x context
	std::vector<EGLint> ctx_attribs = {EGL_CONTEXT_CLIENT_VERSION, gles_major, EGL_NONE};

	const auto context = eglCreateContext(display, config, EGL_NO_CONTEXT, ctx_attribs.data());
	if(context == EGL_NO_CONTEXT) {
		EGL_check("eglCreateContext failed");
	}

	// Surfaceless if supported, else create a tiny pbuffer.
//	const bool has_surfaceless = has_ext(dpy_ext, "EGL_KHR_surfaceless_context");
	const bool has_surfaceless = true;

	EGLSurface surface;
	if(has_surfaceless) {
		surface = EGL_NO_SURFACE;
		if(!eglMakeCurrent(display, surface, surface, context)) {
			EGL_check("eglMakeCurrent (surfaceless) failed");
		}
	} else {
		const EGLint pb_attribs[] = {
			EGL_WIDTH, 1,
			EGL_HEIGHT, 1,
			EGL_NONE
		};
		surface = eglCreatePbufferSurface(display, config, pb_attribs);
		if(surface == EGL_NO_SURFACE) {
			EGL_check("eglCreatePbufferSurface failed");
		}
		if(!eglMakeCurrent(display, surface, surface, context)) {
			EGL_check("eglMakeCurrent (pbuffer) failed");
		}
	}

	EglCtx e;
	e.ctx = context;
	e.display = display;
	e.surface = surface;
	return e;
}

std::string EGL_error_name(EGLint e)
{
	switch (e) {
		case EGL_SUCCESS: return "EGL_SUCCESS";
		case EGL_NOT_INITIALIZED: return "EGL_NOT_INITIALIZED";
		case EGL_BAD_ACCESS: return "EGL_BAD_ACCESS";
		case EGL_BAD_ALLOC: return "EGL_BAD_ALLOC";
		case EGL_BAD_ATTRIBUTE: return "EGL_BAD_ATTRIBUTE";
		case EGL_BAD_CONTEXT: return "EGL_BAD_CONTEXT";
		case EGL_BAD_CONFIG: return "EGL_BAD_CONFIG";
		case EGL_BAD_CURRENT_SURFACE: return "EGL_BAD_CURRENT_SURFACE";
		case EGL_BAD_DISPLAY: return "EGL_BAD_DISPLAY";
		case EGL_BAD_MATCH: return "EGL_BAD_MATCH";
		case EGL_BAD_NATIVE_PIXMAP: return "EGL_BAD_NATIVE_PIXMAP";
		case EGL_BAD_NATIVE_WINDOW: return "EGL_BAD_NATIVE_WINDOW";
		case EGL_BAD_PARAMETER: return "EGL_BAD_PARAMETER";
		case EGL_BAD_SURFACE: return "EGL_BAD_SURFACE";
		default: return "EGL_UNKNOWN_ERROR";
	}
}

void EGL_check(const char* where)
{
	EGLint e = eglGetError();
	if(e != EGL_SUCCESS) {
		die(std::string(where) + ": " + EGL_error_name(e));
	}
}


} // mmpilot

