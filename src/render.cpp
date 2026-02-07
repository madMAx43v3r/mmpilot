/*
 * render.cpp
 *
 *  Created on: Feb 5, 2026
 *      Author: mad
 */

#include <mmpilot/render.h>
#include <mmpilot/helpers.h>


namespace mmpilot {
namespace render {

GLuint g_dummy_vao = 0;
GLuint g_fullscreen_vertex_shader = 0;


void fullscreen()
{
	// a dummy VAO is required in core-ish profiles; ES typically too
	glBindVertexArray(g_dummy_vao);

	glDrawArrays(GL_TRIANGLES, 0, 3);

	GL_check("render::fullscreen()");
}

GLuint get_fullscreen_vertex_shader() {
	return g_fullscreen_vertex_shader;
}

void init()
{
	glGenVertexArrays(1, &g_dummy_vao);

	g_fullscreen_vertex_shader = GL_compile_shader(GL_VERTEX_SHADER, kFullscreenVS);

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
