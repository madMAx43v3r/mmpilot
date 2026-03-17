/*
 * display.cpp
 *
 *  Created on: Feb 11, 2026
 *      Author: mad
 */

#include <mmpilot/display.h>
#include <mmpilot/egl.h>
#include <mmpilot/opengl.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>


namespace mmpilot {

void TexDisplay::main(int width, int height)
{
	Display* xdpy = XOpenDisplay(nullptr);
	if(!xdpy) {
		die("XOpenDisplay failed");
	}

	int screen = DefaultScreen(xdpy);
	Window root = RootWindow(xdpy, screen);

	XSetWindowAttributes swa {};
	swa.event_mask = ExposureMask | KeyPressMask | StructureNotifyMask;
	swa.background_pixel = BlackPixel(xdpy, screen);

	while(width < 640 || height < 480) {
		width *= 2;
		height *= 2;
	}

	int win_w = width, win_h = height;
	Window xwin = XCreateWindow(xdpy, root, 0, 0, win_w, win_h, 0,
			CopyFromParent,
			InputOutput,
			CopyFromParent,
			CWEventMask | CWBackPixel, &swa);

	XStoreName(xdpy, xwin, "EGL GLES Texture Viewer (ESC to quit)");
	Atom wmDelete = XInternAtom(xdpy, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(xdpy, xwin, &wmDelete, 1);
	XMapWindow(xdpy, xwin);

	// ---------------- EGL init ----------------
	EGLDisplay edpy = eglGetDisplay((EGLNativeDisplayType)xdpy);
	if(edpy == EGL_NO_DISPLAY)
		die("eglGetDisplay failed");
	if(!eglInitialize(edpy, nullptr, nullptr))
		die("eglInitialize failed");
	EGL_check("Display: eglInitialize");

	// Request an ES3 context if possible (many drivers accept ES 3.0+ via EGL_OPENGL_ES3_BIT).
	// If your platform only exposes ES2 in EGL config, drop this to EGL_OPENGL_ES2_BIT.
	const EGLint cfgAttribs[] = {
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_ALPHA_SIZE, 8,
		EGL_DEPTH_SIZE, 0,
		EGL_STENCIL_SIZE, 0,
		EGL_NONE
	};

	EGLConfig cfg = nullptr;
	EGLint ncfg = 0;
	if(!eglChooseConfig(edpy, cfgAttribs, &cfg, 1, &ncfg) || ncfg < 1) {
		die("eglChooseConfig failed (no ES3 window config)");
	}

	EGLSurface surf = eglCreateWindowSurface(edpy, cfg, (EGLNativeWindowType)xwin, nullptr);
	if(surf == EGL_NO_SURFACE)
		die("eglCreateWindowSurface failed");
	EGL_check("Display:eglCreateWindowSurface");

	EGLint ctxAttribsES3[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
	EGLContext ctx = eglCreateContext(edpy, cfg, EGL_NO_CONTEXT, ctxAttribsES3);
	if(ctx == EGL_NO_CONTEXT) {
		EGLint ctxAttribsES2[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
		ctx = eglCreateContext(edpy, cfg, EGL_NO_CONTEXT, ctxAttribsES2);
		if(ctx == EGL_NO_CONTEXT)
			die("Display: eglCreateContext failed (ES3 and ES2)");
	}

	if(!eglMakeCurrent(edpy, surf, surf, ctx))
		die("eglMakeCurrent failed");
	EGL_check("Display:eglMakeCurrent");

	GLuint vs = GL_compile_shader(GL_VERTEX_SHADER, "shader/vertex/display.glsl");
	GLuint fs = GL_compile_shader(GL_FRAGMENT_SHADER, "shader/color/display.glsl");
	GLuint prog = GL_link_program(vs, fs);

	GLuint vs_points = GL_compile_shader(GL_VERTEX_SHADER, "shader/vertex/points.glsl");
	GLuint fs_points = GL_compile_shader(GL_FRAGMENT_SHADER, "shader/color/points.glsl");
	GLuint prog_points = GL_link_program(vs_points, fs_points);

	// VAO is required in ES3 core when drawing (even without attributes)
	GLuint vao = 0;
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	// GL_POINTS
	GLuint vao_point = 0;
	GLuint vbo_point = 0;
	{
		glGenVertexArrays(1, &vao_point);
		glBindVertexArray(vao_point);

		float point_xy[2] = {};

		glGenBuffers(1, &vbo_point);
		glBindBuffer(GL_ARRAY_BUFFER, vbo_point);
		glBufferData(GL_ARRAY_BUFFER, sizeof(point_xy), point_xy, GL_DYNAMIC_DRAW);

		// layout(location=0) in vec2 inPos;
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * 4, 0);
	}

	GL_check("Display: setup");

	std::shared_ptr<GL_Tex2D> tex;

	// ---------------- Main loop ----------------
	bool running = true;
	bool resized = true;

	while(running && do_run)
	{
		while(XPending(xdpy))
		{
			XEvent ev;
			XNextEvent(xdpy, &ev);

			if(ev.type == ClientMessage) {
				if((Atom)ev.xclient.data.l[0] == wmDelete)
					running = false;
			} else if(ev.type == ConfigureNotify) {
				int nw = ev.xconfigure.width;
				int nh = ev.xconfigure.height;
				if(nw != win_w || nh != win_h) {
					win_w = nw;
					win_h = nh;
					resized = true;
				}
			} else if(ev.type == KeyPress) {
				KeySym ks = XLookupKeysym(&ev.xkey, 0);
				if(ks == XK_Escape)
					running = false;
			}
		}

		if(do_update) {
			std::lock_guard<std::mutex> lock(mutex);
			tex = std::make_shared<GL_Tex2D>(buf_width, buf_height, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, buffer.data());
			do_update = false;
		}

		if(resized) {
			glViewport(0, 0, win_w, win_h);
			resized = false;
		}
		glUseProgram(prog);

		glClearColor(0.8, 0.4, 0.8, 1);
		glClear(GL_COLOR_BUFFER_BIT);

		glEnable(GL_BLEND);
		glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
							GL_ONE,       GL_ONE_MINUS_SRC_ALPHA);

		if(tex) {
			GL_bind_tex(prog, "uTex", tex, 0);
		}
		glBindVertexArray(vao);
		glDrawArrays(GL_TRIANGLES, 0, 3);

		// markers
		{
			std::lock_guard<std::mutex> lock(mutex);
			if(marker.size())
			{
				for(const auto& entry : marker)
				{
					const auto& marker = entry.second;
					switch(marker.type) {
						case 0:
							float point_xy[2] = {
									2 * marker.x / tex->width,
									2 * marker.y / tex->height
							};

							glBindVertexArray(vao_point);
							glBindBuffer(GL_ARRAY_BUFFER, vbo_point);
							glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(point_xy), point_xy);

							glUseProgram(prog_points);
							GL_uniform_1f(prog_points, "uPointSize", marker.size);
							GL_uniform_4f(prog_points, "uColor", marker.color[0], marker.color[1], marker.color[2], marker.color[3]);
							glDrawArrays(GL_POINTS, 0, 1);

							GL_check("Display: render point");
							break;
					}
				}
			}
		}

		GL_check("Display: render");

		eglSwapBuffers(edpy, surf);

		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}

	// ---------------- Cleanup ----------------
	glDeleteProgram(prog);
	glDeleteProgram(prog_points);

	glDeleteVertexArrays(1, &vao);

	glDeleteBuffers(1, &vbo_point);
	glDeleteVertexArrays(1, &vao_point);

	eglMakeCurrent(edpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	eglDestroyContext(edpy, ctx);
	eglDestroySurface(edpy, surf);
	eglTerminate(edpy);

	XDestroyWindow(xdpy, xwin);
	XCloseDisplay(xdpy);
}


} // mmpilot
