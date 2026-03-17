/*
 * texture.h
 *
 *  Created on: Feb 10, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_TEXTURE_H_
#define INCLUDE_MMPILOT_TEXTURE_H_

#include <mmpilot/opengl.h>

#include <memory>
#include <vector>
#include <stdexcept>


namespace mmpilot {

class GL_Tex2D {
public:
	GLuint id = 0;
	GLsizei width = 0;
	GLsizei height = 0;
	GLenum internal_fmt = 0;
	GLenum format = 0;
	GLenum type = 0;

	GL_Tex2D(GLsizei w, GLsizei h, GLenum internal_fmt, GLenum fmt, GLenum type, const void* data = nullptr)
		:	width(w), height(h), internal_fmt(internal_fmt), format(fmt), type(type)
	{
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

		id = GL_create_tex(w, h, internal_fmt, fmt, type, data);
	}

	GL_Tex2D(const GL_Tex2D&) = delete;
	GL_Tex2D& operator=(const GL_Tex2D&) = delete;

	~GL_Tex2D() {
		glDeleteTextures(1, &id);
	}

	std::shared_ptr<GL_Tex2D> clone() {
		return std::make_shared<GL_Tex2D>(width, height, internal_fmt, format, type);
	}

	void bind() {
		glBindTexture(GL_TEXTURE_2D, id);
	}

	void upload(const void* data, int stride = 0)
	{
		bind();
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glPixelStorei(GL_UNPACK_ROW_LENGTH, stride);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, format, type, data);
		GL_check("GL_Tex2D::upload()");
	}

	template<typename T>
	void download(std::vector<T>& out) const
	{
		auto fbo = GL_create_FBO(id);
		switch(format) {
			case GL_RED:	GL_read_FBO_R(fbo, 0, width, height, out); break;
			case GL_RG:		GL_read_FBO_RG(fbo, 0, width, height, out); break;
			case GL_RGBA:	GL_read_FBO_RGBA(fbo, 0, width, height, out); break;
			default:
				throw std::runtime_error("unsupported texture type");
		}
		glDeleteFramebuffers(1, &fbo);
		GL_check("GL_Tex2D::download()");
	}

	std::vector<uint8_t> download_u8() const
	{
		std::vector<uint8_t> out;
		download(out);
		return out;
	}

	std::vector<float> download_f32() const
	{
		std::vector<float> out;
		download(out);
		return out;
	}

};


inline void GL_bind_tex(GLuint prog, const char* name, std::shared_ptr<GL_Tex2D> tex, GLint unit) {
	GL_bind_tex(prog, name, tex->id, unit);
}

inline GLuint GL_create_FBO(std::shared_ptr<GL_Tex2D> tex) {
	return GL_create_FBO(tex->id);
}

inline GLuint GL_create_FBO(const std::vector<std::shared_ptr<GL_Tex2D>>& tex) {
	std::vector<GLuint> ids;
	for(auto t : tex) {
		ids.push_back(t->id);
	}
	return GL_create_FBO(ids);
}

inline void GL_blit(
		std::shared_ptr<GL_Tex2D> dst, std::shared_ptr<GL_Tex2D> src)
{
	GLuint fbo[2] = {};
	glGenFramebuffers(2, fbo);

	glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo[0]);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo[1]);

	glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, src->id, 0);
	glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, dst->id, 0);

	const GLenum buf = GL_COLOR_ATTACHMENT0;
	glDrawBuffers(1, &buf);

	GLenum mode = GL_NEAREST;
	if(dst->width != src->width || dst->height != src->height) {
		mode = GL_LINEAR;
	}
	glBlitFramebuffer(
		0, 0, src->width, src->height,
		0, 0, dst->width, dst->height,
		GL_COLOR_BUFFER_BIT,
		mode
	);
	GL_finish("GL_blit_FBO()");

	glDeleteFramebuffers(2, fbo);
}


} // mmpilot

#endif /* INCLUDE_MMPILOT_TEXTURE_H_ */
