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

#include <vector>
#include <iostream>


namespace mmpilot {

class Mapping {
public:
	Transform2D state;

	std::shared_ptr<GL_Tex2D> tex_map;
	std::shared_ptr<GL_Tex2D> tex_weight;
	std::shared_ptr<GL_Tex2D> tex_debug;

	void init(int width, int height, GLenum format);

	void update(const Transform2D& delta);

	void render(std::shared_ptr<GL_Tex2D> img, const Homography::Params& H);

	std::shared_ptr<GL_Tex2D> finalize();

private:
	struct Node {
		Transform2D pose;
		std::shared_ptr<GL_Tex2D> image;
	};

	void add_node();

	void render_image(
			std::shared_ptr<GL_Tex2D> img, const std::vector<Vec3f>& coords,
			const GLuint fbo, const int width_, const int height_, bool do_clear);

private:
	int width = 0;
	int height = 0;

	std::vector<Node> nodes;

	GLuint prog_map = 0;
	GLuint prog_out = 0;

	GLuint fbo_map = 0;
	GLuint fbo_debug = 0;

	GLuint vao = 0;
	GLuint vbo = 0;
	GLuint ebo = 0;

	GLenum format = 0;

	bool have_init = false;
	bool need_clear = true;
	bool is_mono = false;

};





} // mmpilot

#endif /* INCLUDE_MMPILOT_MAPPING_H_ */
