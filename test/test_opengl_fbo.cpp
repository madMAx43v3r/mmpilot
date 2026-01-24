/*
 * test_opengl_fbo.cpp
 *
 *  Created on: Jan 24, 2026
 *      Author: mad
 */

// gles_fbo_probe.cpp
// Build: g++ -std=c++17 -O2 gles_fbo_probe.cpp -o gles_fbo_probe -lEGL -lGLESv2
//
// Runs headless using EGL_KHR_surfaceless_context / EGL_MESA_platform_surfaceless.

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl31.h>

#include <array>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

static void die(const std::string& msg) {
    std::cerr << "ERROR: " << msg << "\n";
    std::exit(1);
}

static std::string eglErrorToString(EGLint e) {
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

static std::string glErrorToString(GLenum e) {
    switch (e) {
        case GL_NO_ERROR: return "GL_NO_ERROR";
        case GL_INVALID_ENUM: return "GL_INVALID_ENUM";
        case GL_INVALID_VALUE: return "GL_INVALID_VALUE";
        case GL_INVALID_OPERATION: return "GL_INVALID_OPERATION";
        case GL_INVALID_FRAMEBUFFER_OPERATION: return "GL_INVALID_FRAMEBUFFER_OPERATION";
        case GL_OUT_OF_MEMORY: return "GL_OUT_OF_MEMORY";
        default: return "GL_UNKNOWN_ERROR";
    }
}

static void checkEgl(const char* where) {
    EGLint e = eglGetError();
    if (e != EGL_SUCCESS) {
        die(std::string(where) + ": " + eglErrorToString(e));
    }
}

static void checkGl(const char* where) {
    GLenum e = glGetError();
    if (e != GL_NO_ERROR) {
        die(std::string(where) + ": " + glErrorToString(e));
    }
}

static std::string fboStatusToString(GLenum s) {
    switch (s) {
        case GL_FRAMEBUFFER_COMPLETE: return "GL_FRAMEBUFFER_COMPLETE";
        case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT: return "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT";
        case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT: return "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT";
        case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS: return "GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS";
        case GL_FRAMEBUFFER_UNSUPPORTED: return "GL_FRAMEBUFFER_UNSUPPORTED";
        case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE: return "GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE";
        default: return "GL_FRAMEBUFFER_UNKNOWN_STATUS";
    }
}

struct EglCtx {
    EGLDisplay dpy = EGL_NO_DISPLAY;
    EGLContext ctx = EGL_NO_CONTEXT;

    ~EglCtx() {
        if (dpy != EGL_NO_DISPLAY) {
            eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            if (ctx != EGL_NO_CONTEXT) {
                eglDestroyContext(dpy, ctx);
            }
            eglTerminate(dpy);
        }
    }
};

static EglCtx initEglSurfacelessGles31() {
    EglCtx e;

    // Prefer surfaceless platform if available
    // eglGetPlatformDisplay is EGL 1.5 or EGL_EXT_platform_base
    auto eglGetPlatformDisplayEXT =
        (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");

    if (!eglGetPlatformDisplayEXT) {
        die("eglGetPlatformDisplayEXT not available; ensure EGL_EXT_platform_base is present.");
    }

    e.dpy = eglGetPlatformDisplayEXT(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, nullptr);
    if (e.dpy == EGL_NO_DISPLAY) {
        checkEgl("eglGetPlatformDisplayEXT");
        die("Failed to get surfaceless EGL display.");
    }

    if (!eglInitialize(e.dpy, nullptr, nullptr)) {
        checkEgl("eglInitialize");
        die("eglInitialize failed");
    }

    // Bind OpenGL ES API
    if (!eglBindAPI(EGL_OPENGL_ES_API)) {
        checkEgl("eglBindAPI");
        die("eglBindAPI(EGL_OPENGL_ES_API) failed");
    }

    // Choose any config that supports pbuffer (we won't actually create a surface)
    const EGLint cfgAttribs[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_NONE
    };

    EGLConfig cfg = nullptr;
    EGLint num = 0;
    if (!eglChooseConfig(e.dpy, cfgAttribs, &cfg, 1, &num) || num < 1) {
        checkEgl("eglChooseConfig");
        die("No suitable EGLConfig found for GLES3.");
    }

    // Create GLES 3.1 context
    const EGLint ctxAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 3,
        // Some drivers allow minor version attribute; most ignore it.
        EGL_NONE
    };

    e.ctx = eglCreateContext(e.dpy, cfg, EGL_NO_CONTEXT, ctxAttribs);
    if (e.ctx == EGL_NO_CONTEXT) {
        checkEgl("eglCreateContext");
        die("eglCreateContext failed");
    }

    // Make current with no surfaces (surfaceless)
    if (!eglMakeCurrent(e.dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, e.ctx)) {
        checkEgl("eglMakeCurrent");
        die("eglMakeCurrent failed (surfaceless).");
    }

    return e;
}

struct FormatCase {
    const char* name;
    GLint internalFormat;
    GLenum format;
    GLenum type;
};

static bool testFormat(const FormatCase& fc, bool verbose = false) {
    // Create 1x1 texture
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexImage2D(GL_TEXTURE_2D, 0, fc.internalFormat, 1, 1, 0, fc.format, fc.type, nullptr);
    GLenum ge = glGetError();
    if (ge != GL_NO_ERROR) {
        if (verbose) {
            std::cerr << "  glTexImage2D error: " << glErrorToString(ge) << "\n";
        }
        glDeleteTextures(1, &tex);
        return false;
    }

    // Create FBO and attach
    GLuint fbo = 0;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        if (verbose) {
            std::cerr << "  FBO status: " << fboStatusToString(status) << "\n";
        }
        glDeleteFramebuffers(1, &fbo);
        glDeleteTextures(1, &tex);
        return false;
    }

    // Ensure draw buffer is set (important on GLES)
    GLenum drawBuf = GL_COLOR_ATTACHMENT0;
	glDrawBuffers(1, &drawBuf);
	checkGl("glDrawBuffers");

    // Try a clear + readback to ensure writes work
    glViewport(0, 0, 1, 1);
	glDisable(GL_SCISSOR_TEST);
	glClearColor(0.25f, 0.5f, 0.75f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

    // Validate by reading back
	bool ok = false;
	if (fc.type == GL_UNSIGNED_BYTE) {
		unsigned char pix[4] = {0,0,0,0};
		glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pix);
		GLenum ge = glGetError();
		if (ge == GL_NO_ERROR) {
			// Expect approx [64,128,191,255]
			ok = (pix[3] > 200); // alpha should be near 255
		}
	} else {
		float pix[4] = {0,0,0,0};
		glReadPixels(0, 0, 1, 1, GL_RGBA, GL_FLOAT, pix);
		GLenum ge = glGetError();
		if (ge == GL_NO_ERROR) {
			// Loose tolerance for conversion/quantization
			ok = (pix[3] > 0.9f);
		}
	}

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &tex);
    return ok;
}

static void printGlInfo() {
    auto s = [](GLenum e) -> const char* {
        const GLubyte* p = glGetString(e);
        return p ? reinterpret_cast<const char*>(p) : "(null)";
    };

    std::cout << "GL_RENDERER: " << s(GL_RENDERER) << "\n";
    std::cout << "GL_VERSION : " << s(GL_VERSION) << "\n";
    std::cout << "GL_VENDOR  : " << s(GL_VENDOR) << "\n";
    std::cout << "GLSL       : " << s(GL_SHADING_LANGUAGE_VERSION) << "\n";

    GLint maxDB = 0, maxCA = 0;
    glGetIntegerv(GL_MAX_DRAW_BUFFERS, &maxDB);
    glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &maxCA);
    std::cout << "GL_MAX_DRAW_BUFFERS     : " << maxDB << "\n";
    std::cout << "GL_MAX_COLOR_ATTACHMENTS: " << maxCA << "\n";

    GLint maxWG[3] = {0,0,0};
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, &maxWG[0]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 1, &maxWG[1]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 2, &maxWG[2]);
    std::cout << "GL_MAX_COMPUTE_WORK_GROUP_SIZE: "
              << maxWG[0] << " " << maxWG[1] << " " << maxWG[2] << "\n";
}

int main() {
    EglCtx egl = initEglSurfacelessGles31();
    printGlInfo();

    // Test cases. Some may not be valid/allowed on GLES implementations; that is the point.
    const std::vector<FormatCase> cases = {
        {"RGBA8",   GL_RGBA8,   GL_RGBA, GL_UNSIGNED_BYTE},

        {"RGBA16F", GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT},
        {"RG16F",   GL_RG16F,   GL_RG,   GL_HALF_FLOAT},
        {"R16F",    GL_R16F,    GL_RED,  GL_HALF_FLOAT},

        {"RGBA32F", GL_RGBA32F, GL_RGBA, GL_FLOAT},
        {"RG32F",   GL_RG32F,   GL_RG,   GL_FLOAT},
        {"R32F",    GL_R32F,    GL_RED,  GL_FLOAT},

        // RGB float formats are often not renderable on GLES; may fail at teximage or FBO.
        {"RGB16F",  GL_RGB16F,  GL_RGB,  GL_HALF_FLOAT},
        {"RGB32F",  GL_RGB32F,  GL_RGB,  GL_FLOAT},
    };

    std::cout << "\nFBO renderability tests (1x1 texture attached to COLOR_ATTACHMENT0):\n";
    std::cout << std::left << std::setw(10) << "Format" << "  Result\n";
    std::cout << "-------------------------\n";

    for (const auto& fc : cases) {
        bool ok = testFormat(fc, /*verbose=*/false);
        std::cout << std::left << std::setw(10) << fc.name << "  " << (ok ? "OK" : "FAIL") << "\n";
    }

    std::cout << "\nTip: If RG16F/R16F fail, keep using RGBA16F and pack into channels.\n";
    return 0;
}



