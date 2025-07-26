#pragma once

#include <glad/glad.h>

namespace Fetcko {
class VertexArray {
public:
	// TODO: Expand this out and place it somewhere
	//       more accessible
	static const inline std::map<GLenum, std::size_t> Sizes = {
		{ GL_FLOAT, sizeof(float) }
	};

	struct Attribute {
		Attribute() = default;
		Attribute(GLuint index, GLint size) :
			index(index),
			size(size) {
			stride = static_cast<GLsizei>(size * Sizes.at(type));
		}

		Attribute(GLuint index, GLint size, GLsizei stride) :
			index(index),
			size(size),
			stride(stride) {
		}

		Attribute(GLuint index, GLint size, GLsizei stride, std::size_t offset) :
			index(index),
			size(size),
			stride(stride) {
			pointer = reinterpret_cast<void *>(offset);
		}

		GLuint index = GL_MAX_VERTEX_ATTRIBS;
		GLint size;
		GLenum type = GL_FLOAT;
		GLboolean normalized = GL_FALSE;
		GLsizei stride; 
		const void *pointer = nullptr;

		void Enable() {
			glEnableVertexAttribArray(index);
		}
		void Disable() {
			glDisableVertexAttribArray(index);
		}
	};

	VertexArray() {
		glGenVertexArrays(1, &handle);
	}
	~VertexArray() {
		glDeleteVertexArrays(1, &handle);
	}

	void AddAttribute(Attribute &&attribute) {
		attribute.Enable();

		glVertexAttribPointer(
			attribute.index,
			attribute.size,
			attribute.type,
			attribute.normalized,
			attribute.stride,
			attribute.pointer
		);

		attributes.emplace_back(std::move(attribute));
	}

	void Bind() {
		glBindVertexArray(handle);
	}

	void Unbind() {
		glBindVertexArray(0);
	}

	const GLuint &GetHandle() const { return handle; }
private:
	GLuint handle = 0;

	std::vector<Attribute> attributes;
};
}