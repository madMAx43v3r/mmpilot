/*
 * helpers.cpp
 *
 *  Created on: Feb 5, 2026
 *      Author: mad
 */

#include <mmpilot/helpers.h>

#include <fstream>
#include <string>
#include <stdexcept>
#include <iostream>


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

GLuint GL_compile_shader(GLenum type, const std::string& src)
{
	GLuint sh = glCreateShader(type);
	glShaderSource(sh, 1, src.data(), nullptr);
	glCompileShader(sh);

	GLint ok = 0;
	glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
	if(!ok) {
		GLint len = 0;
		glGetShaderiv(sh, GL_INFO_LOG_LENGTH, &len);
		std::string log(len, '\0');
		glGetShaderInfoLog(sh, len, nullptr, log.data());
		std::fprintf(stderr, "Shader compile error:\n%s\n", log.c_str());
		die("glCompileShader failed");
	}
	GL_check("GL_compile_shader");
	return sh;
}

GLuint GL_compile_shader_file(GLenum type, const std::string& file_path)
{
	return GL_compile_shader(type, read_file_txt(file_path));
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

std::string read_file_txt(const std::string& path)
{
	std::ifstream file(path, std::ios::binary);
	if(!file) {
		throw std::runtime_error("Failed to open file: " + path);
	}
	return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}



} // mmpilot


