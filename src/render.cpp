/*
 * render.cpp
 *
 *  Created on: Feb 5, 2026
 *      Author: mad
 */

#include <mmpilot/render.h>
#include <mmpilot/opengl.h>


namespace mmpilot {
namespace render {

GLuint g_dummy_vao = 0;
GLuint g_fullscreen_vertex_shader = 0;


void fullscreen(GLuint fbo, int width, int height)
{
	glDisable(GL_BLEND);
	glDisable(GL_SCISSOR_TEST);

	fullscreen_ex(fbo, width, height);
}

void fullscreen_ex(GLuint fbo, int width, int height)
{
	glDisable(GL_DEPTH_TEST);

	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glViewport(0, 0, width, height);

	// a dummy VAO is required in core-ish profiles; ES typically too
	glBindVertexArray(g_dummy_vao);

	glDrawArrays(GL_TRIANGLES, 0, 3);

	GL_check("render::fullscreen_ex()");
}

void clear(GLuint fbo, int width, int height, const std::array<float, 4>& color, const float depth)
{
	glDisable(GL_SCISSOR_TEST);
	glClearDepthf(depth);
	glClearColor(color[0], color[1], color[2], color[3]);
	glViewport(0, 0, width, height);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	GL_finish("render::clear()");
}

GLuint get_fullscreen_vertex_shader() {
	return g_fullscreen_vertex_shader;
}

void init()
{
	glGenVertexArrays(1, &g_dummy_vao);

	g_fullscreen_vertex_shader =
			GL_compile_shader(GL_VERTEX_SHADER, "shader/vertex/fullscreen.glsl");

	GL_check("render::init()");
}

void cleanup()
{
	glDeleteShader(g_fullscreen_vertex_shader);

	glDeleteVertexArrays(1, &g_dummy_vao);

	GL_check("render::cleanup()");
}


} // render
} // mmpilot
