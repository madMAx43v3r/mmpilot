/*
 * helpers.h
 *
 *  Created on: Feb 5, 2026
 *      Author: mad
 */

#ifndef INCLUDE_HELPERS_H_
#define INCLUDE_HELPERS_H_

#include <GLES3/gl31.h>
#include <stdexcept>
#include <string>
#include <sstream>


namespace mmpilot {

inline void die(const char* msg) {
	throw std::runtime_error(msg);
}

const char* GL_error_name(GLenum err);

void GL_check(const char* where);

void GL_finish();
void GL_finish(const char* where);

GLuint GL_compile_shader(GLenum type, const std::string& src);

GLuint GL_compile_shader_file(GLenum type, const std::string& file_path);

GLuint GL_link_program(GLuint vs, GLuint fs);

std::string read_file_txt(const std::string& path);


} // mmpilot

#endif /* INCLUDE_HELPERS_H_ */
