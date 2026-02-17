/*
 * mapping.cpp
 *
 *  Created on: Feb 17, 2026
 *      Author: mad
 */

#include <mmpilot/mapping.h>
#include <mmpilot/render.h>


namespace mmpilot {

void Mapping::init(GLenum format)
{
	if(have_init) {
		throw std::logic_error("already initialized");
	}
	state.pos = Vec2f(width / 2, height / 2);

	GLenum int_format_out;
	GLenum int_format_map;
	std::string shader;
	switch(format) {
		case GL_RG:
			int_format_out = GL_RG8;
			int_format_map = GL_RG16F;
			shader = "render_mono.glsl";
			break;
		case GL_RGBA:
			int_format_out = GL_RGBA8;
			int_format_map = GL_RGBA16F;
			shader = "render_rgba.glsl";
			break;
		default:
			throw std::logic_error("Mapping: invalid format");
	}

	out        = std::make_shared<GL_Tex2D>(width, height, int_format_out, format, GL_UNSIGNED_BYTE);
	tex_map    = std::make_shared<GL_Tex2D>(width, height, int_format_map, format, GL_HALF_FLOAT);
	tex_weight = std::make_shared<GL_Tex2D>(width, height, GL_R16F, GL_RED, GL_HALF_FLOAT);

	fbo = GL_create_FBO({tex_map->id, tex_weight->id});
	fbo_out = GL_create_FBO(out->id);

	{
		const auto vs = GL_compile_shader(GL_VERTEX_SHADER,   "shader/mapping/vertex.glsl");
		const auto fs = GL_compile_shader(GL_FRAGMENT_SHADER, "shader/mapping/" + shader);
		prog = GL_link_program(vs, fs);
	}
	{
		const auto vs = render::get_fullscreen_vertex_shader();
		const auto fs = GL_compile_shader(GL_FRAGMENT_SHADER, "shader/mapping/compress.glsl");
		prog_out = GL_link_program(vs, fs);
	}

	const float vert[4 * 5] = {0};	// dummy
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
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), 0);

	// layout(location=1) in vec2 inUV;
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(2 * sizeof(float)));

	// layout(location=2) in float inHW;
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(4 * sizeof(float)));

	have_init = true;
}

void Mapping::update(const Transform2D& delta)
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

void Mapping::render(std::shared_ptr<GL_Tex2D> img, const Homography::Params& H)
{
	if(!have_init) {
		throw std::logic_error("not initialized");
	}
	const float w = img->width;
	const float h = img->height;
	const float uv[4][2] = {{0,0}, {1,0}, {1,1}, {0,1}};
	const Vec2f center = Vec2f(w, h) / 2;

	float xmin = 0;
	float ymin = 0;
	float xmax = width;
	float ymax = height;
	float vert[4 * 5] = {};

	std::cout << "Mapping render: " << std::endl;

	for(int i = 0; i < 4; ++i)
	{
		const auto u = uv[i][0];
		const auto v = uv[i][1];
		const auto q = H.project3(Vec2f(w * u, h * v) - center);

		const Vec2f p = state.apply(q.x(), q.y());

		std::cout << "  " << p.transpose() << " / " << u << " " << v << " / " << q.z() << std::endl;

		vert[i * 5 + 0] = p.x();
		vert[i * 5 + 1] = p.y();
		vert[i * 5 + 2] = u;
		vert[i * 5 + 3] = v;
		vert[i * 5 + 4] = q.z();

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

	GL_uniform_2f(prog, "uMapSize", width, height);

	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glViewport(0, 0, width, height);

	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_ONE, GL_ONE);

	glEnable(GL_SCISSOR_TEST);
	glScissor(xmin, ymin, (xmax - xmin) + 1, (ymax - ymin) + 1);

	glBindVertexArray(vao);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);

	GL_finish("Mapping::render()");
}

void Mapping::finalize()
{
	if(!have_init) {
		throw std::logic_error("not initialized");
	}
	glUseProgram(prog_out);

	GL_bind_tex(prog_out, "uMap", tex_map->id, 0);
	GL_bind_tex(prog_out, "uWeight", tex_weight->id, 1);

	render::fullscreen(fbo_out, width, height);

	GL_finish("Mapping::finalize()");
}


} // mmpilot
