/*
 * opengl.h
 *
 *  Created on: Feb 8, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_OPENGL_H_
#define INCLUDE_MMPILOT_OPENGL_H_

#include <GLES3/gl31.h>
#include <string>
#include <stdexcept>


namespace mmpilot {

const char* GL_error_name(GLenum err);

void GL_check(const char* where);

void GL_finish();
void GL_finish(const char* where);

GLuint GL_compile_shader(GLenum type, const std::string& src);

GLuint GL_compile_shader_file(GLenum type, const std::string& file_path);

GLuint GL_link_program(GLuint vs, GLuint fs);


} // mmpilot

#endif /* INCLUDE_MMPILOT_OPENGL_H_ */
