#pragma once

#include <glad/glad.h>

namespace Fetcko {
class Texture {
public:
	Texture() = delete;
	Texture(const Texture &) = delete;
	Texture(Texture &&other) noexcept {
		handle = std::move(other.handle);
		other.handle = 0;
		internalFormat = std::move(other.internalFormat);
		format = std::move(other.format);
	}

	Texture(GLint internalFormat = GL_RGBA, GLenum format = GL_RGBA, bool bind = false) :
		internalFormat(internalFormat),
		format(format) {
		glGenTextures(1, &handle);
		if (bind) Bind();
	}
	~Texture() {
		if (handle > 0)
			glDeleteTextures(1, &handle);
	}

	void Bind() const {
		glBindTexture(GL_TEXTURE_2D, handle);
	}

	void Unbind() const {
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	void TexImage2D(GLsizei width, GLsizei height, const void *data) const {
		glTexImage2D(
			GL_TEXTURE_2D,
			0,
			internalFormat,
			width,
			height,
			0,
			format,
			GL_UNSIGNED_BYTE,
			data
		);
	}

	void SetTexParameters(GLint wrapParam = GL_CLAMP_TO_EDGE, GLint filterParam = GL_LINEAR) const {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapParam);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapParam);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filterParam);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filterParam);
	}

	const GLuint &GetHandle() const { return handle; }

private:
	GLuint handle = 0;

	GLint internalFormat = GL_RGBA;
	GLenum format = GL_RGBA;
};
}