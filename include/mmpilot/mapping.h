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
	int width = 2 * 1024;
	int height = 2 * 1024;

	Transform2D state;

	std::shared_ptr<GL_Tex2D> tex_map;
	std::shared_ptr<GL_Tex2D> tex_weight;

	void init(GLenum int_format, GLenum format, GLenum type)
	{
		if(have_init) {
			throw std::logic_error("already initialized");
		}
		state.pos = Vec2f(width / 2, height / 2);

		tex_map = std::make_shared<GL_Tex2D>(width, height, int_format, format, type);
		tex_weight = std::make_shared<GL_Tex2D>(width, height, GL_R16F, GL_RED, GL_HALF_FLOAT);
		fbo = GL_create_FBO({tex_map->id, tex_weight->id});

//		const auto vs = GL_compile_shader(GL_VERTEX_SHADER,   "shader/weight/radius.glsl");
//		const auto fs = GL_compile_shader(GL_FRAGMENT_SHADER, "shader/weight/radius.glsl");
//		prog = GL_link_program(vs, fs);

		have_init = true;
	}

	void update(const Transform2D& delta)
	{
		if(!have_init) {
			throw std::logic_error("not initialized");
		}
		state.add(delta);

		std::cout << "Mapping delta = " << delta.pos.transpose()
				<< ", rot = " << rad2deg(get_angle(delta.rot))
				<< ", scale = " << delta.scale << std::endl;

		std::cout << "Mapping pos   = " << state.pos.transpose()
				<< ", rot = " << rad2deg(get_angle(state.rot))
				<< ", scale = " << state.scale << std::endl;
	}

	void render(std::shared_ptr<GL_Tex2D> img, const Homography::Params& H)
	{
		const float w = img->width;
		const float h = img->height;
		const Vec2f center = Vec2f(w, h) / 2;

		Vec2f coords[4] = {
				H.project(Vec2f(0, 0) - center),	// lower left
				H.project(Vec2f(w, 0) - center),	// lower right
				H.project(Vec2f(w, h) - center),	// upper right
				H.project(Vec2f(0, h) - center)		// upper left
		};

		std::cout << "Mapping render: " << std::endl;
		for(int i = 0; i < 4; ++i) {
			std::cout << "  " << coords[i].transpose() << std::endl;
		}

	}

private:
	GLuint prog = 0;
	GLuint fbo = 0;

	bool have_init = false;

};





} // mmpilot

#endif /* INCLUDE_MMPILOT_MAPPING_H_ */
