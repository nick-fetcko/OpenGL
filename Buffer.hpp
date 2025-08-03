#pragma once

#include <array>
#include <map>
#include <typeindex>
#include <vector>

#include <glad/glad.h>

namespace Fetcko {
template<
	GLenum E,
	typename T,
	typename = typename std::enable_if<std::is_arithmetic<T>::value, T>::type
>
class Buffer {
public:
	Buffer() {
		glGenBuffers(1, &handle);
	}
	~Buffer() {
		glDeleteBuffers(1, &handle);
	}

	inline void Bind() {
		glBindBuffer(E, handle);
	}
	inline void Unbind() {
		glBindBuffer(E, 0);
	}

	void BufferData(const T *data, std::size_t size, GLenum usage = GL_STATIC_DRAW) {
		this->size = size;
		glBufferData(E, sizeof(T) * size, data, usage);
	}

	void BufferData(const std::vector<T> &data, GLenum usage = GL_STATIC_DRAW) {
		size = data.size();
		glBufferData(E, sizeof(T) * data.size(), data.data(), usage);
	}

	template <std::size_t N>
	void BufferData(const std::array<T, N> data, GLenum usage = GL_STATIC_DRAW) {
		size = N;
		glBufferData(E, sizeof(T) * N, data.data(), usage);
	}

	// Allocates an _empty_ buffer of the provided size
	void BufferData(std::size_t size, GLenum usage = GL_STATIC_DRAW) {
		this->size = size;
		glBufferData(E, size, nullptr, usage);
	}

	void BufferSubData(GLintptr offset, GLsizeiptr size, const void *data) {
		glBufferSubData(E, offset * sizeof(T), size, data);
	}

	std::vector<T> GetBufferSubData(GLintptr offset, GLsizeiptr size) {
		std::vector<T> ret(size);
		glGetBufferSubData(E, offset * sizeof(T), size * sizeof(T), ret.data());
		return ret;
	}

	inline constexpr void DrawArrays(GLenum mode, GLint first, GLsizei count) {
		glDrawArrays(mode, first, count);
	}

	inline constexpr void DrawElements(GLenum mode, const void *indices = nullptr) const {
		if constexpr (std::is_same<T, float>::value)
			glDrawElements(mode, static_cast<GLsizei>(size), GL_FLOAT, indices);
		else if constexpr (std::is_same<T, unsigned short>::value)
			glDrawElements(mode, static_cast<GLsizei>(size), GL_UNSIGNED_SHORT, indices);
	}

	const GLuint &GetHandle() const { return handle; }
private:
	GLuint handle = 0;

	typename std::vector<T>::size_type size = 0;
};

using ArrayBuffer = Buffer<GL_ARRAY_BUFFER, float>;
using ElementBuffer = Buffer<GL_ELEMENT_ARRAY_BUFFER, unsigned short>;
}