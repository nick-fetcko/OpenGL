#pragma once

#include <glad/glad.h>

namespace Fetcko {
template<GLenum E>
class Texture {
public:
	Texture(const Texture &) = delete;
	Texture(Texture &&other) noexcept {
		handle = std::move(other.handle);
		other.handle = 0;
		internalFormat = std::move(other.internalFormat);
		format = std::move(other.format);
	}

	Texture &operator=(Texture &&right) noexcept {
		handle = right.handle;
		right.handle = 0;
		internalFormat = right.internalFormat;
		format = right.format;

		return *this;
	}

	explicit Texture(GLint internalFormat = GL_RGBA, GLenum format = GL_RGBA, bool bind = false) :
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
		glBindTexture(E, handle);
	}

	void Unbind() const {
		glBindTexture(E, 0);
	}

	template<
		GLenum _E = E,
		typename std::enable_if<_E == GL_TEXTURE_2D, bool> * = nullptr
	>
	void TexImage2D(GLsizei width, GLsizei height, const void *data, GLenum type = GL_UNSIGNED_BYTE) const {
		glTexImage2D(
			E,
			0,
			internalFormat,
			width,
			height,
			0,
			format,
			type,
			data
		);
	}

	template<
		GLenum _E = E,
		typename std::enable_if<_E == GL_TEXTURE_3D, bool> * = nullptr
	>
	void TexImage3D(GLsizei width, GLsizei height, GLsizei depth, const void *data, GLenum type = GL_UNSIGNED_BYTE) const {
		glTexImage3D(
			E,
			0,
			internalFormat,
			width,
			height,
			depth,
			0,
			format,
			type,
			data
		);
	}

	template<
		GLenum _E = E,
		typename std::enable_if_t<_E == GL_TEXTURE_2D_MULTISAMPLE, bool> * = nullptr
	>
	void TexImage2DMultisample(GLsizei width, GLsizei height) const {
		glTexImage2DMultisample(
			E,
			4,
			internalFormat,
			width,
			height,
			GL_TRUE
		);
	}

	template<
		GLenum _E = E,
		typename std::enable_if_t<_E == GL_TEXTURE_2D, bool> * = nullptr
	>
	void SetTexParameters(GLint wrapParam = GL_CLAMP_TO_EDGE, GLint filterParam = GL_LINEAR) const {
		glTexParameteri(E, GL_TEXTURE_WRAP_S, wrapParam);
		glTexParameteri(E, GL_TEXTURE_WRAP_T, wrapParam);
		glTexParameteri(E, GL_TEXTURE_MIN_FILTER, filterParam);
		glTexParameteri(E, GL_TEXTURE_MAG_FILTER, filterParam);
	}

	const GLuint &GetHandle() const { return handle; }

private:
	GLuint handle = 0;

	GLint internalFormat = GL_RGBA;
	GLenum format = GL_RGBA;
};

using Texture2D = Texture<GL_TEXTURE_2D>;
using Texture3D = Texture<GL_TEXTURE_3D>;
using MultisampledTexture2D = Texture<GL_TEXTURE_2D_MULTISAMPLE>;
}