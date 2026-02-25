/*
 * render.h
 *
 *  Created on: Feb 5, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_RENDER_H_
#define INCLUDE_MMPILOT_RENDER_H_

#include <GLES3/gl31.h>

#include <array>


namespace mmpilot {
namespace render {

void fullscreen(GLuint fbo, int width, int height);

void fullscreen_ex(GLuint fbo, int width, int height);

void clear(GLuint fbo, int width, int height, const std::array<float, 4>& color = {}, const float depth = 1);

GLuint get_fullscreen_vertex_shader();

void init();

void cleanup();


} // render
} // mmpilot

#endif /* INCLUDE_MMPILOT_RENDER_H_ */
