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
			width, height, is_mono ? GL_R8 : GL_RGB8, is_mono ? GL_RED : GL_RGB, GL_UNSIGNED_BYTE);

//	weight = std::make_shared<GL_Tex2D>(width, height, GL_R16F, GL_RED, GL_HALF_FLOAT);

	fbo = GL_create_FBO(map);

	glGenRenderbuffers(1, &rbo);
	glBindRenderbuffer(GL_RENDERBUFFER, rbo);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);

	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);

	clear();
}

Mapping::Buffer::~Buffer()
{
	glDeleteFramebuffers(1, &fbo);
	glDeleteRenderbuffers(1, &rbo);
}

void Mapping::Buffer::clear()
{
	render::clear(fbo, map->width, map->height, {}, 0);
}

void Mapping::init(int width_, int height_, GLenum format)
{
	if(have_init) {
		throw std::logic_error("already initialized");
	}
	width = width_;
	height = height_;

	std::string s_render;
	switch(format) {
		case GL_RG:
			is_mono = true;
			s_render = "render_mono.glsl";
			break;
		case GL_RGBA:
			is_mono = false;
			s_render = "render_rgba.glsl";
			break;
		default:
			throw std::logic_error("Mapping: invalid format");
	}

	merge.init(width, height, format);

	tex_tmp = std::make_shared<GL_Tex2D>(
			width, height, is_mono ? GL_RG8 : GL_RGBA8, is_mono ? GL_RG : GL_RGBA, GL_UNSIGNED_BYTE);

	tex_debug = std::make_shared<GL_Tex2D>(
			width, height, is_mono ? GL_R8 : GL_RGB8, is_mono ? GL_RED : GL_RGB, GL_UNSIGNED_BYTE);

	fbo_debug = GL_create_FBO(tex_debug->id);

	{
		const auto vs = GL_compile_shader(GL_VERTEX_SHADER,   "shader/mapping/vertex.glsl");
		const auto fs = GL_compile_shader(GL_FRAGMENT_SHADER, "shader/mapping/" + s_render);
		prog_render = GL_link_program(vs, fs);
	}
	{
		const auto vs = render::get_fullscreen_vertex_shader();
		const auto fs = GL_compile_shader(GL_FRAGMENT_SHADER, "shader/mapping/compress.glsl");
		prog_compress = GL_link_program(vs, fs);
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

void Mapping::update(std::shared_ptr<GL_Tex2D> img, const Homography::Params& H)
{
	if(!have_init) {
		throw std::logic_error("not initialized");
	}
	if(img->width != width || img->height != height) {
		throw std::logic_error("dimension mismatch");
	}
	const auto delta = H.transform();

	auto fixed = delta;
	fixed.scale /= scale_bias;

	state.add(fixed);
	{
		const float a = 0.05;
		const float x = std::clamp(delta.scale, 0.95f, 1.05f);
		scale_bias *= std::pow(x / scale_bias, a);
	}
	const auto image = std::make_shared<GL_Tex2D>(
			width, height, is_mono ? GL_RG8 : GL_RGBA8, is_mono ? GL_RG : GL_RGBA, GL_UNSIGNED_BYTE);

	GL_blit(image, img);

	Node node;
	node.H = H;
	node.pose = state;
	node.image = image;

	if(nodes.size()) {
		optimize(nodes.back(), node);
	}
	nodes.push_back(node);

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
		throw std::logic_error("not initialized");
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
//	render_image(buffer, img, coords);

	if(tex_debug) {
//		compress(fbo_debug, buffer);
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
	const int width = buf->map->width;
	const int height = buf->map->height;

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

	glUseProgram(prog_render);

	GL_bind_tex(prog_render, "uSrc", img->id, 0);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	GL_uniform_2f(prog_render, "uMapSize", width, height);

	glBindFramebuffer(GL_FRAMEBUFFER, buf->fbo);
	glViewport(0, 0, width, height);

	glEnable(GL_SCISSOR_TEST);
	glScissor(xmin - 1, ymin - 1, (xmax - xmin) + 2, (ymax - ymin) + 2);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_GREATER);
	glDepthMask(GL_TRUE);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, buf->rbo);

	glDisable(GL_BLEND);

	glBindVertexArray(vao);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);

	GL_finish("Mapping::render_image()");

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_SCISSOR_TEST);
}

void Mapping::compress(GLuint fbo, std::shared_ptr<Buffer> buf)
{
	glUseProgram(prog_compress);

	GL_bind_tex(prog_compress, "uMap", buf->map->id, 0);
	GL_bind_tex(prog_compress, "uWeight", buf->weight->id, 1);

	render::fullscreen(fbo, buf->map->width, buf->map->height);

	GL_finish("Mapping::compress()");
}

void Mapping::optimize(Node& L, Node& R)
{
	const auto w_sum = L.weight + R.weight;

	merge.weight = R.weight / w_sum;
	merge.exec(L.image, R.image, R.H);

	GL_blit(tex_tmp, merge.out);

	merge.weight = L.weight / w_sum;
	merge.exec(R.image, L.image, R.H.inverse());

	GL_blit(L.image, merge.out);
	GL_blit(R.image, tex_tmp);

	L.weight = w_sum;
	R.weight = w_sum;
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
		const auto w = node.pose.scale * width;
		const auto h = node.pose.scale * height;
		xmin = std::min(xmin, p.x() - w / 2);
		ymin = std::min(ymin, p.y() - h / 2);
		xmax = std::max(xmax, p.x() + w / 2);
		ymax = std::max(ymax, p.y() + h / 2);
	}
	const Vec2f origin = Vec2f(xmin, ymin);

	const int map_width  = (xmax - xmin);
	const int map_height = (ymax - ymin);

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

//	auto out = std::make_shared<GL_Tex2D>(
//			map_width, map_height, is_mono ? GL_R8 : GL_RGB8, is_mono ? GL_RED : GL_RGB, GL_UNSIGNED_BYTE);
//	const auto fbo = GL_create_FBO(out->id);
//
//	compress(fbo, buf);
//
//	glDeleteFramebuffers(1, &fbo);

	return buf->map;
}




} // mmpilot
