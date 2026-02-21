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
	struct Node {
		Transform2D pose;
		std::shared_ptr<GL_Tex2D> image;
	};

	struct Buffer {
		GLuint fbo = 0;
		std::shared_ptr<GL_Tex2D> map;
		std::shared_ptr<GL_Tex2D> weight;		// write
		std::shared_ptr<GL_Tex2D> weight_read;	// read-back

		Buffer(int width, int height, bool is_mono);
		~Buffer();
		void mirror();
		void clear();
	private:
		GLuint fbo_copy[2] = {};
	};

	Transform2D state;

	std::shared_ptr<Buffer> buffer;

	std::shared_ptr<GL_Tex2D> tex_debug;

	void init(int width, int height, GLenum format);

	void update(const Transform2D& delta);

	void render(std::shared_ptr<GL_Tex2D> img, const Homography::Params& H);

	std::shared_ptr<GL_Tex2D> finalize();

private:
	void add_node();

	void render_image(
			std::shared_ptr<Buffer> buf,
			std::shared_ptr<GL_Tex2D> img,
			const std::vector<Vec2f>& coords);

private:
	int width = 0;
	int height = 0;

	bool have_init = false;
	bool is_mono = false;

	float scale_bias = 1;

	std::vector<Node> nodes;

	GLuint prog = 0;

	GLuint vao = 0;
	GLuint vbo = 0;
	GLuint ebo = 0;

};





} // mmpilot

#endif /* INCLUDE_MMPILOT_MAPPING_H_ */
