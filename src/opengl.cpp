/*
 * opengl.cpp
 *
 *  Created on: Feb 8, 2026
 *      Author: mad
 */

#include <mmpilot/opengl.h>
#include <mmpilot/util.h>

#include <stdexcept>
#include <iostream>
#include <sstream>


namespace mmpilot {

const char* GL_error_name(GLenum err)
{
    switch(err) {
        case GL_NO_ERROR:                     return "GL_NO_ERROR";
        case GL_INVALID_ENUM:                 return "GL_INVALID_ENUM";
        case GL_INVALID_VALUE:                return "GL_INVALID_VALUE";
        case GL_INVALID_OPERATION:            return "GL_INVALID_OPERATION";
        case GL_INVALID_FRAMEBUFFER_OPERATION:return "GL_INVALID_FRAMEBUFFER_OPERATION";
        case GL_OUT_OF_MEMORY:                return "GL_OUT_OF_MEMORY";
        default:                              return "UNKNOWN_GL_ERROR";
    }
}

std::string GL_FBO_status_name(GLenum s) {
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

void GL_check(const char* where)
{
    GLenum err;
    bool found = false;
    std::ostringstream oss;

    while((err = glGetError()) != GL_NO_ERROR) {
        if(!found) {
            oss << "OpenGL error(s) at " << where << ":";
            found = true;
        }
        oss << "\n  - " << GL_error_name(err) << " (0x" << std::hex << err << std::dec << ")";
    }
    if(found) {
        throw std::runtime_error(oss.str());
    }
}

void GL_print_version()
{
	const char* ver = reinterpret_cast<const char*>(glGetString(GL_VERSION));
	const char* ren = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
	const char* ven = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
	std::cout << "GL_VENDOR  : " << (ven ? ven : "(null)") << "\n";
	std::cout << "GL_RENDERER: " << (ren ? ren : "(null)") << "\n";
	std::cout << "GL_VERSION : " << (ver ? ver : "(null)") << "\n";
	GL_check("glGetString()");
}

void GL_print_precision()
{
	GLint range[2], precision;
	glGetShaderPrecisionFormat(GL_FRAGMENT_SHADER, GL_HIGH_FLOAT, range, &precision);
	std::cout << "GL_HIGH_FLOAT = " << precision << std::endl;
	glGetShaderPrecisionFormat(GL_FRAGMENT_SHADER, GL_MEDIUM_FLOAT, range, &precision);
	std::cout << "GL_MEDIUM_FLOAT = " << precision << std::endl;
	glGetShaderPrecisionFormat(GL_FRAGMENT_SHADER, GL_LOW_FLOAT, range, &precision);
	std::cout << "GL_LOW_FLOAT = " << precision << std::endl;
	GL_check("GL_print_precision()");
}

void GL_finish()
{
	glFinish();
	GL_check("glFinish()");
}

void GL_finish(const char* where)
{
	glFinish();
	GL_check(where);
}

GLuint GL_compile_shader_source(GLenum type, const std::string& source, const std::string& name)
{
	GLuint sh = glCreateShader(type);
	const char* src = source.data();
	GLint src_len = source.size();
	glShaderSource(sh, 1, &src, &src_len);
	glCompileShader(sh);

	GLint ok = 0;
	glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
	if(!ok) {
		std::cout << source << std::endl;

		GLint len = 0;
		glGetShaderiv(sh, GL_INFO_LOG_LENGTH, &len);
		len = std::max(len, 4096);

		GLsizei outLen = 0;
		std::vector<char> log(len);
		glGetShaderInfoLog(sh, len, &outLen, log.data());
		std::cerr << "Shader compile error: " << name << std::endl
				<< std::string(log.data(), outLen).c_str() << std::endl;
		die("glCompileShader failed");
	}
	GL_check("GL_compile_shader");
	return sh;
}

GLuint GL_compile_shader(GLenum type, const std::string& file_path)
{
	return GL_compile_shader_source(type, read_file_txt(file_path), file_path);
}

GLuint GL_link_program(GLuint vs, GLuint fs)
{
	GLuint prog = glCreateProgram();
	glAttachShader(prog, vs);
	glAttachShader(prog, fs);
	glLinkProgram(prog);

	GLint ok = 0;
	glGetProgramiv(prog, GL_LINK_STATUS, &ok);
	if(!ok) {
		GLint len = 0;
		glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
		std::string log(len, '\0');
		glGetProgramInfoLog(prog, len, nullptr, log.data());
		std::fprintf(stderr, "Program link error:\n%s\n", log.c_str());
		die("linkProgram failed");
	}
	GL_check("GL_link_program");
	return prog;
}

GLuint GL_create_tex(GLsizei w, GLsizei h, GLenum internal_fmt, GLenum fmt, GLenum type, const void* data)
{
	GLuint tex = 0;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glTexImage2D(GL_TEXTURE_2D, 0, internal_fmt, w, h, 0, fmt, type, data);

	GL_check("GL_create_tex");
	return tex;
}

GLuint GL_create_FBO(const GLuint tex) {
	return GL_create_FBO(std::vector<GLuint>{tex});
}

GLuint GL_create_FBO(const std::vector<GLuint>& tex_list)
{
	GLuint fbo = 0;
	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);

	std::vector<GLenum> bufs;
	for(size_t i = 0; i < tex_list.size(); ++i) {
		glFramebufferTexture2D(GL_FRAMEBUFFER,
				GL_COLOR_ATTACHMENT0 + (int)i,
				GL_TEXTURE_2D, tex_list[i], 0);
		bufs.push_back(GL_COLOR_ATTACHMENT0 + (int)i);
	}
	glDrawBuffers(bufs.size(), bufs.data());

	const auto status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if(status != GL_FRAMEBUFFER_COMPLETE) {
		die("FBO incomplete: " + GL_FBO_status_name(status));
	}
	GL_check("GL_create_FBO");
	return fbo;
}

void GL_bind_tex(GLuint prog, const char* name, GLuint tex, GLint unit)
{
	auto loc = glGetUniformLocation(prog, name);
	if(loc < 0) {
		return; // allow unused
	}
	glActiveTexture(GL_TEXTURE0 + unit);
	glBindTexture(GL_TEXTURE_2D, tex);
	glUniform1i(loc, unit);

	GL_check("GL_bind_tex");
}

void GL_uniform_1f(GLuint prog, const char* name, float x)
{
	auto loc = glGetUniformLocation(prog, name);
	if(loc >= 0) {
		glUniform1f(loc, x);
	}
}

void GL_uniform_2f(GLuint prog, const char* name, float x, float y)
{
	auto loc = glGetUniformLocation(prog, name);
	if(loc >= 0) {
		glUniform2f(loc, x, y);
	}
}

void GL_uniform_1i(GLuint prog, const char* name, int v)
{
	auto loc = glGetUniformLocation(prog, name);
	if(loc >= 0) {
		glUniform1i(loc, v);
	}
}

void GL_uniform_2i(GLuint prog, const char* name, int x, int y)
{
	auto loc = glGetUniformLocation(prog, name);
	if(loc >= 0) {
		glUniform2i(loc, x, y);
	}
}

void GL_read_FBO_R(GLuint fbo, int index, int w, int h, std::vector<float>& out)
{
	out.resize(w * h);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glReadBuffer(GL_COLOR_ATTACHMENT0 + index);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glPixelStorei(GL_PACK_ROW_LENGTH, 0);
	glReadPixels(0, 0, w, h, GL_RED, GL_FLOAT, out.data());
	GL_check("GL_read_FBO_R(GL_FLOAT)");
}

void GL_read_FBO_R(GLuint fbo, int index, int w, int h, std::vector<uint8_t>& out)
{
	out.resize(w * h);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glReadBuffer(GL_COLOR_ATTACHMENT0 + index);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glPixelStorei(GL_PACK_ROW_LENGTH, 0);
	glReadPixels(0, 0, w, h, GL_RED, GL_UNSIGNED_BYTE, out.data());
	GL_check("GL_read_FBO_R(GL_UNSIGNED_BYTE)");
}

void GL_read_FBO_RG(GLuint fbo, int index, int w, int h, std::vector<float>& out)
{
	out.resize(w * h * 2);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glReadBuffer(GL_COLOR_ATTACHMENT0 + index);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glPixelStorei(GL_PACK_ROW_LENGTH, 0);
	glReadPixels(0, 0, w, h, GL_RG, GL_FLOAT, out.data());
	GL_check("GL_read_FBO_RG(GL_FLOAT)");
}

void GL_read_FBO_RG(GLuint fbo, int index, int w, int h, std::vector<uint8_t>& out)
{
	out.resize(w * h * 2);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glReadBuffer(GL_COLOR_ATTACHMENT0 + index);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glPixelStorei(GL_PACK_ROW_LENGTH, 0);
	glReadPixels(0, 0, w, h, GL_RG, GL_UNSIGNED_BYTE, out.data());
	GL_check("GL_read_FBO_RG(GL_UNSIGNED_BYTE)");
}

void GL_read_FBO_RGBA(GLuint fbo, int index, int w, int h, std::vector<float>& out)
{
	out.resize(w * h * 4);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glReadBuffer(GL_COLOR_ATTACHMENT0 + index);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glPixelStorei(GL_PACK_ROW_LENGTH, 0);
	glReadPixels(0, 0, w, h, GL_RGBA, GL_FLOAT, out.data());
	GL_check("GL_read_FBO_RGBA(GL_FLOAT)");
}

void GL_read_FBO_RGBA(GLuint fbo, int index, int w, int h, std::vector<uint8_t>& out)
{
	out.resize(w * h * 4);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glReadBuffer(GL_COLOR_ATTACHMENT0 + index);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glPixelStorei(GL_PACK_ROW_LENGTH, 0);
	glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, out.data());
	GL_check("GL_read_FBO_RGBA(GL_UNSIGNED_BYTE)");
}


} // mmpilot

