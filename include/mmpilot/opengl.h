/*
 * opengl.h
 *
 *  Created on: Feb 8, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_OPENGL_H_
#define INCLUDE_MMPILOT_OPENGL_H_

#include <GLES3/gl31.h>

#include <array>
#include <string>
#include <vector>


namespace mmpilot {

const char* GL_error_name(GLenum err);

void GL_check(const char* where);

void GL_finish();
void GL_finish(const char* where);

GLuint GL_compile_shader(GLenum type, const std::string& src);

GLuint GL_compile_shader_file(GLenum type, const std::string& file_path);

GLuint GL_link_program(GLuint vs, GLuint fs);

GLuint GL_create_tex(GLsizei w, GLsizei h, GLenum internal_fmt, GLenum fmt, GLenum type);

GLuint GL_create_FBO(const std::vector<GLuint>& tex_list);

void GL_bind_tex(GLuint prog, const char* name, GLuint tex, GLint unit);

void GL_set_uniform_2f(GLuint prog, const char* name, float x, float y);

void GL_set_uniform_int(GLuint prog, const char* name, int v);

template<int N>
void GL_set_uniform_fv(GLuint prog, const char* name, const std::array<float, N>& v);

void GL_read_FBO_R(GLuint fbo, int index, int w, int h, std::vector<float>& out);

void GL_read_FBO_RG(GLuint fbo, int index, int w, int h, std::vector<float>& out);

void GL_read_FBO_RGBA(GLuint fbo, int index, int w, int h, std::vector<float>& out);


} // mmpilot

#endif /* INCLUDE_MMPILOT_OPENGL_H_ */
