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

Mapping::Buffer::Buffer(int width, int height, bool is_mono)
{
	map = std::make_shared<GL_Tex2D>(
			width, height, is_mono ? GL_RG16F : GL_RGBA16F, is_mono ? GL_RG : GL_RGBA, GL_HALF_FLOAT);

	weight      = std::make_shared<GL_Tex2D>(width, height, GL_R16F, GL_RED, GL_HALF_FLOAT);
	weight_read = std::make_shared<GL_Tex2D>(width, height, GL_R16F, GL_RED, GL_HALF_FLOAT);

	fbo = GL_create_FBO({map->id, weight->id});

	fbo_copy[0] = GL_create_FBO(weight->id);
	fbo_copy[1] = GL_create_FBO(weight_read->id);

	clear();
}

Mapping::Buffer::~Buffer()
{
	glDeleteFramebuffers(1, &fbo);
	glDeleteFramebuffers(2, fbo_copy);
}

void Mapping::Buffer::mirror()
{
	glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo_copy[0]);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo_copy[1]);
	glBlitFramebuffer(
		0, 0, weight->width, weight->height,
		0, 0, weight_read->width, weight_read->height,
		GL_COLOR_BUFFER_BIT, GL_NEAREST
	);
	GL_finish("Buffer::mirror()");
}

void Mapping::Buffer::clear()
{
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glViewport(0, 0, map->width, map->height);
	glDisable(GL_BLEND);
	glDisable(GL_SCISSOR_TEST);
	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT);
	GL_finish("Buffer::clear()");

	mirror();
}

void Mapping::init(int width_, int height_, GLenum format)
{
	if(have_init) {
		throw std::logic_error("already initialized");
	}
	width = width_;
	height = height_;

	std::string shader;
	switch(format) {
		case GL_RG:
			is_mono = true;
			shader = "render_mono.glsl";
			break;
		case GL_RGBA:
			is_mono = false;
			shader = "render_rgba.glsl";
			break;
		default:
			throw std::logic_error("Mapping: invalid format");
	}

	buffer = std::make_shared<Buffer>(width, height, is_mono);

	tex_debug = std::make_shared<GL_Tex2D>(
			width, height, is_mono ? GL_R8 : GL_RGB8, is_mono ? GL_RED : GL_RGB, GL_UNSIGNED_BYTE);

	{
		const auto vs = GL_compile_shader(GL_VERTEX_SHADER,   "shader/mapping/vertex.glsl");
		const auto fs = GL_compile_shader(GL_FRAGMENT_SHADER, "shader/mapping/" + shader);
		prog = GL_link_program(vs, fs);
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
	render_image(buffer, img, coords);

	if(tex_debug) {
		GL_blit_FBO(tex_debug, buffer->map);
	}
}

void Mapping::render_image(
		std::shared_ptr<Buffer> buf,
		std::shared_ptr<GL_Tex2D> img,
		const std::vector<Vec2f>& coords)
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

	glUseProgram(prog);

	GL_bind_tex(prog, "uSrc", img->id, 0);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	GL_bind_tex(prog, "uWeight", buf->weight_read->id, 1);

	GL_uniform_2f(prog, "uMapSize", buf->map->width, buf->map->height);

	glBindFramebuffer(GL_FRAMEBUFFER, buf->fbo);
	glViewport(0, 0, buf->map->width, buf->map->height);

	glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);

	glEnable(GL_SCISSOR_TEST);
	glScissor(xmin, ymin, (xmax - xmin) + 1, (ymax - ymin) + 1);

	glBindVertexArray(vao);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);

	GL_finish("Mapping::render_image()");

	glDisable(GL_SCISSOR_TEST);

	buf->mirror();
}

void Mapping::add_node()
{
	const auto image = std::make_shared<GL_Tex2D>(
			width, height, is_mono ? GL_RG8 : GL_RGBA8, is_mono ? GL_RG : GL_RGBA, GL_UNSIGNED_BYTE);

	GL_blit_FBO(image, buffer->map);

	Node node;
	node.pose = state;
	node.image = image;
	nodes.push_back(node);

	buffer->clear();
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

	auto buf = std::make_shared<Buffer>(map_width, map_height, is_mono);

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
		render_image(buf, node.image, coords);
	}

	auto out = std::make_shared<GL_Tex2D>(
			map_width, map_height, is_mono ? GL_R8 : GL_RGB8, is_mono ? GL_RED : GL_RGB, GL_UNSIGNED_BYTE);

	GL_blit_FBO(out, buf->map);

	return out;
}




} // mmpilot
