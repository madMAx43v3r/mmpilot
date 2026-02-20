/*
 * mapping.cpp
 *
 *  Created on: Feb 17, 2026
 *      Author: mad
 */

#include <mmpilot/mapping.h>
#include <mmpilot/render.h>

#include <limits>


namespace mmpilot {

static const Vec2f g_uv[4] = {{0,0}, {1,0}, {1,1}, {0,1}};


void Mapping::init(int width_, int height_, GLenum format)
{
	if(have_init) {
		throw std::logic_error("already initialized");
	}
	width = width_;
	height = height_;

	if(format == GL_RED) {
		format = GL_RG;
	}
	if(format == GL_RGB) {
		format = GL_RGBA;
	}
	std::string shader;
	switch(format) {
		case GL_RG:
			is_mono = true;
			shader = "render_mono.glsl";
			break;
		case GL_RGBA:
			shader = "render_rgba.glsl";
			break;
		default:
			throw std::logic_error("Mapping: invalid format");
	}

	tex_map     = std::make_shared<GL_Tex2D>(
			width, height, is_mono ? GL_RG16F : GL_RGBA16F, format, GL_HALF_FLOAT);
	tex_weight  = std::make_shared<GL_Tex2D>(
			width, height, GL_R16F, GL_RED, GL_HALF_FLOAT);
	tex_debug   = std::make_shared<GL_Tex2D>(
			width, height, is_mono ? GL_R8 : GL_RGB8, is_mono ? GL_RED : GL_RGB, GL_UNSIGNED_BYTE);

	fbo_map = GL_create_FBO({tex_map->id, tex_weight->id});
	fbo_debug = GL_create_FBO(tex_debug->id);

	{
		const auto vs = GL_compile_shader(GL_VERTEX_SHADER,   "shader/mapping/vertex.glsl");
		const auto fs = GL_compile_shader(GL_FRAGMENT_SHADER, "shader/mapping/" + shader);
		prog_map = GL_link_program(vs, fs);
	}
	{
		const auto vs = render::get_fullscreen_vertex_shader();
		const auto fs = GL_compile_shader(GL_FRAGMENT_SHADER, "shader/mapping/compress.glsl");
		prog_out = GL_link_program(vs, fs);
	}

	const float vert[4 * 4] = {0};	// dummy
	const uint16_t vert_idx[6] = {0, 1, 2, 0, 2, 3};

	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vert), vert, GL_DYNAMIC_DRAW);

	glGenBuffers(1, &ebo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(vert_idx), vert_idx, GL_STATIC_DRAW);

	// layout(location=0) in vec2 inPos;
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * 4, 0);

	// layout(location=1) in vec2 inUV;
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * 4, (void*)(2 * 4));

	have_init = true;
}

void Mapping::update(const Transform2D& delta)
{
	if(!have_init) {
		throw std::logic_error("not initialized");
	}
	add_node();

	auto fixed = delta;
	fixed.scale /= scale_bias;

	state.add(fixed);
	{
		const float a = 0.05;
		const float x = std::clamp(delta.scale, 0.95f, 1.05f);
		scale_bias *= std::pow(x / scale_bias, a);
	}

	std::cout << "Mapping delta = " << delta.pos.transpose()
			<< ", rot = " << get_angle_deg(delta.rot)
			<< " deg, scale = " << delta.scale << std::endl;

	std::cout << "Mapping pos   = " << state.pos.transpose()
			<< ", rot = " << get_angle_deg(state.rot)
			<< " deg, scale = " << state.scale << ", bias = " << scale_bias << std::endl;
}

void Mapping::render(std::shared_ptr<GL_Tex2D> img, const Homography::Params& H)
{
	if(!have_init) {
		init((img->width * 5) / 4, (img->height * 5) / 4, img->format);
	}
	const float w = img->width;
	const float h = img->height;
	const Vec2f c_img = Vec2f(w, h) / 2;
	const Vec2f c_map = Vec2f(width, height) / 2;

	std::vector<Vec2f> coords;
	for(int i = 0; i < 4; ++i) {
		const auto& uv = g_uv[i];
		const Vec2f p = c_map + H.project(Vec2f(w * uv.x(), h * uv.y()) - c_img);
		coords.push_back(p);
	}
	render_image(img, coords, fbo_map, width, height, need_clear);

	need_clear = false;

	glUseProgram(prog_out);

	GL_bind_tex(prog_out, "uMap", tex_map->id, 0);
	GL_bind_tex(prog_out, "uWeight", tex_weight->id, 1);

	render::fullscreen(fbo_debug, width, height);

	GL_finish("Mapping::render()");
}

void Mapping::render_image(
		std::shared_ptr<GL_Tex2D> img, const std::vector<Vec2f>& coords,
		const GLuint fbo, const int width_, const int height_, bool do_clear)
{
	if(coords.size() != 4) {
		throw std::logic_error("render_image(): coords.size() != 4");
	}
	float xmin = std::numeric_limits<float>::max();
	float ymin = std::numeric_limits<float>::max();
	float xmax = std::numeric_limits<float>::min();
	float ymax = std::numeric_limits<float>::min();

	float vert[4 * 4] = {};

	for(int i = 0; i < 4; ++i)
	{
		const auto& p = coords[i];
		vert[i * 4 + 0] = p.x();
		vert[i * 4 + 1] = p.y();
		vert[i * 4 + 2] = g_uv[i].x();
		vert[i * 4 + 3] = g_uv[i].y();

		xmin = std::min(xmin, p.x());
		ymin = std::min(ymin, p.y());
		xmax = std::max(xmax, p.x());
		ymax = std::max(ymax, p.y());
	}

	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vert), vert);

	glUseProgram(prog_map);

	GL_bind_tex(prog_map, "uSrc", img->id, 0);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	GL_uniform_2f(prog_map, "uMapSize", width_, height_);

	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glViewport(0, 0, width_, height_);

	if(do_clear) {
		glDisable(GL_BLEND);
		glDisable(GL_SCISSOR_TEST);
		glClearColor(0, 0, 0, 0);
		glClear(GL_COLOR_BUFFER_BIT);
	}
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_ONE, GL_ONE);

	glEnable(GL_SCISSOR_TEST);
	glScissor(xmin, ymin, (xmax - xmin) + 1, (ymax - ymin) + 1);

	glBindVertexArray(vao);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);

	GL_finish("Mapping::render_image()");
}

void Mapping::add_node()
{
	const auto img = std::make_shared<GL_Tex2D>(
			width, height, is_mono ? GL_RG8 : GL_RGBA8, is_mono ? GL_RG : GL_RGBA, GL_UNSIGNED_BYTE);
	const auto fbo = GL_create_FBO(img->id);

	glUseProgram(prog_out);

	GL_bind_tex(prog_out, "uMap", tex_map->id, 0);
	GL_bind_tex(prog_out, "uWeight", tex_weight->id, 1);

	render::fullscreen(fbo, width, height);

	GL_finish("Mapping::add_node()");

	glDeleteFramebuffers(1, &fbo);

	Node node;
	node.pose = state;
	node.image = img;
	nodes.push_back(node);

	need_clear = true;
}

std::shared_ptr<GL_Tex2D> Mapping::finalize()
{
	if(nodes.empty()) {
		return nullptr;
	}
	float xmin = std::numeric_limits<float>::max();
	float ymin = std::numeric_limits<float>::max();
	float xmax = std::numeric_limits<float>::min();
	float ymax = std::numeric_limits<float>::min();

	for(const auto& node : nodes) {
		const auto& p = node.pose.pos;
		xmin = std::min(xmin, p.x());
		ymin = std::min(ymin, p.y());
		xmax = std::max(xmax, p.x());
		ymax = std::max(ymax, p.y());
	}
	const Vec2f origin = Vec2f(xmin - width / 2, ymin - height / 2);

	const int map_width  = (xmax - origin.x()) + width / 2 + 1;
	const int map_height = (ymax - origin.y()) + height / 2 + 1;

	std::cout << "Mapping: map size = " << map_width << " x " << map_height << ", nodes = " << nodes.size() << std::endl;

	auto map = std::make_shared<GL_Tex2D>(
			map_width, map_height, is_mono ? GL_R16F : GL_RGB16F, is_mono ? GL_RED : GL_RGB, GL_HALF_FLOAT);
	auto weight = std::make_shared<GL_Tex2D>(
			map_width, map_height, GL_R16F, GL_RED, GL_HALF_FLOAT);
	auto fbo_map = GL_create_FBO({map->id, weight->id});

	bool do_clear = true;
	for(const auto& node : nodes)
	{
		std::vector<Vec2f> coords;
		for(int i = 0; i < 4; ++i)
		{
			const auto& uv = g_uv[i];
			const Vec2f q = Vec2f(width * uv.x(), height * uv.y()) - Vec2f(width, height) / 2;
			const Vec2f p = node.pose.apply(q) - origin;
			coords.push_back(p);
		}
		render_image(node.image, coords, fbo_map, map_width, map_height, do_clear);
		do_clear = false;
	}

	auto out = std::make_shared<GL_Tex2D>(
			map_width, map_height, is_mono ? GL_R8 : GL_RGB8, is_mono ? GL_RED : GL_RGB, GL_UNSIGNED_BYTE);
	auto fbo_out = GL_create_FBO(out->id);

	glUseProgram(prog_out);

	GL_bind_tex(prog_out, "uMap", map->id, 0);
	GL_bind_tex(prog_out, "uWeight", weight->id, 1);

	render::fullscreen(fbo_out, map_width, map_height);

	GL_finish("Mapping::finalize()");

	glDeleteFramebuffers(1, &fbo_map);
	glDeleteFramebuffers(1, &fbo_out);

	return out;
}




} // mmpilot
