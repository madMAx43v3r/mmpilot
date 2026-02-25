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
#include <mmpilot/merge.h>

#include <vector>
#include <iostream>


namespace mmpilot {

class Mapping {
public:
	struct Node {
		Transform2D pose;
		Homography::Params H;
		std::shared_ptr<GL_Tex2D> image;
		float weight = 1;
	};

	struct Buffer {
		std::shared_ptr<GL_Tex2D> map;
		std::shared_ptr<GL_Tex2D> weight;

		GLuint fbo = 0;
		GLuint rbo = 0;

		Buffer(int width, int height, bool is_mono);
		~Buffer();
		void clear();
	};

	MergeFilter merge;

	Transform2D state;

	std::shared_ptr<GL_Tex2D> tex_tmp;
	std::shared_ptr<GL_Tex2D> tex_debug;

	void init(int width, int height, GLenum format);

	void update(std::shared_ptr<GL_Tex2D> img, const Homography::Params& H);

	void render(std::shared_ptr<GL_Tex2D> img, const Homography::Params& H);

	void optimize(Node& L, Node& R);

	std::shared_ptr<GL_Tex2D> finalize();

private:
	void render_image(
			std::shared_ptr<Buffer> buf,
			std::shared_ptr<GL_Tex2D> img,
			const std::vector<Vec2f>& coords);

	void compress(GLuint fbo, std::shared_ptr<Buffer> buf);

private:
	int width = 0;
	int height = 0;

	bool have_init = false;
	bool is_mono = false;

	float scale_bias = 1;

	std::vector<Node> nodes;

	GLuint prog_render = 0;
	GLuint prog_compress = 0;

	GLuint vao = 0;
	GLuint vbo = 0;
	GLuint ebo = 0;

	GLuint fbo_debug = 0;

};





} // mmpilot

#endif /* INCLUDE_MMPILOT_MAPPING_H_ */
