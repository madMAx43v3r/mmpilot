/*
 * texture.h
 *
 *  Created on: Feb 10, 2026
 *      Author: mad
 */

#ifndef INCLUDE_MMPILOT_TEXTURE_H_
#define INCLUDE_MMPILOT_TEXTURE_H_

#include <mmpilot/opengl.h>


namespace mmpilot {

class GL_Tex2D {
public:
	GLuint id = 0;
	GLsizei width = 0;
	GLsizei height = 0;
	GLenum internal_fmt = 0;
	GLenum format = 0;
	GLenum type = 0;

	explicit GL_Tex2D(GLsizei w, GLsizei h, GLenum internal_fmt, GLenum fmt, GLenum type)
		:	width(w), height(h), internal_fmt(internal_fmt), format(fmt), type(type)
	{
		id = GL_create_tex(w, h, internal_fmt, fmt, type);
	}

	~GL_Tex2D() {
		glDeleteTextures(1, &id);
	}

	void bind() {
		glBindTexture(GL_TEXTURE_2D, id);
	}

	void upload(const void* data, int level = 0)
	{
		bind();
		if(width % 4 == 0) {
			glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
		} else {
			glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		}
		glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
		glTexSubImage2D(GL_TEXTURE_2D, level, 0, 0, width, height, format, type, data);
		GL_check("GL_Tex2D::upload()");
	}

};


} // mmpilot

#endif /* INCLUDE_MMPILOT_TEXTURE_H_ */
