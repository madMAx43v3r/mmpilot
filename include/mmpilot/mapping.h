/*
 * mapping.h
 *
 *  Created on: Feb 17, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_MAPPING_H_
#define INCLUDE_MMPILOT_MAPPING_H_

#include <mmpilot/opengl.h>
#include <mmpilot/texture.h>
#include <mmpilot/homography.h>
#include <mmpilot/transform.h>

#include <iostream>


namespace mmpilot {

class Mapping {
public:
	int width = 4096;
	int height = 4096;

	Transform2D state;

	std::shared_ptr<GL_Tex2D> out;
	std::shared_ptr<GL_Tex2D> tex_map;
	std::shared_ptr<GL_Tex2D> tex_weight;

	void init(GLenum format);

	void update(const Transform2D& delta);

	void render(std::shared_ptr<GL_Tex2D> img, const Homography::Params& H);

	void finalize();

private:
	GLuint prog = 0;
	GLuint prog_out = 0;

	GLuint fbo = 0;
	GLuint fbo_out = 0;

	GLuint vao = 0;
	GLuint vbo = 0;
	GLuint ebo = 0;

	bool have_init = false;

};





} // mmpilot

#endif /* INCLUDE_MMPILOT_MAPPING_H_ */
