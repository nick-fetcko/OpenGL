#pragma once

#include <glad/glad.h>

#include "Utils.hpp"
#include "Logger.hpp"

namespace Fetcko {
template <GLenum E>
class Shader : public LoggableClass {
public:
	Shader() {
		handle = glCreateShader(E);
	}

	Shader(Shader &&other) noexcept {
		handle = other.handle;
		other.handle = 0;
	}

	~Shader() {
		glDeleteShader(handle);
	}

	bool Compile(const std::filesystem::path &path) {
		auto string = Utils::GetStringFromFile(path);
		auto source = string.c_str();

		int ret;

		glShaderSource(handle, 1, &source, nullptr);
		glCompileShader(handle);

		glGetShaderiv(handle, GL_COMPILE_STATUS, &ret);

		if (!ret) {
			GLint size = 0;
			glGetShaderiv(handle, GL_INFO_LOG_LENGTH, &size);
			
			std::vector<char> infoLog(size);
			glGetShaderInfoLog(handle, size, nullptr, infoLog.data());

			LogError("Shader compilation failed:\n", std::string(infoLog.begin(), infoLog.end()));
		}

		return ret != 0;
	}

	const GLuint &GetHandle() const { return handle; }

private:
	GLuint handle;
};

using VertexShader = Shader<GL_VERTEX_SHADER>;
using FragmentShader = Shader<GL_FRAGMENT_SHADER>;
}