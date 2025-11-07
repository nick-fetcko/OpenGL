#include "ShaderProgram.hpp"

namespace Fetcko {
ShaderProgram::ShaderProgram() {
	handle = glCreateProgram();
}

ShaderProgram::ShaderProgram(ShaderProgram &&other) noexcept {
	handle = other.handle;
	other.handle = 0;

	vertexShader = std::move(other.vertexShader);
	fragmentShaders = std::move(other.fragmentShaders);

	uniforms = std::move(other.uniforms);
}

ShaderProgram::~ShaderProgram() {
	glDeleteProgram(handle);
}

void ShaderProgram::Attach(
	const VertexShader &vertexShader,
	const std::vector<FragmentShader> &fragmentShaders
) {
	this->vertexShader = vertexShader;
	this->fragmentShaders = fragmentShaders;

	glAttachShader(handle, vertexShader.GetHandle());

	for (const auto &fragment : fragmentShaders)
		glAttachShader(handle, fragment.GetHandle());

	int ret;

	glLinkProgram(handle);
	glGetProgramiv(handle, GL_LINK_STATUS, &ret);

	if (!ret) {
		GLint size = 0;
		glGetProgramiv(handle, GL_INFO_LOG_LENGTH, &size);

		std::vector<char> infoLog(size);
		glGetProgramInfoLog(handle, size, nullptr, infoLog.data());

		LogError("Shader linking failed:\n", std::string(infoLog.begin(), infoLog.end()));
	}
}

void ShaderProgram::Use() const {
	glUseProgram(handle);
}

const GLuint &ShaderProgram::GetHandle() const {
	return handle;
}
const GLuint ShaderProgram::GetUniformBlockIndex(const std::string_view &uniformBlock) const {
	return glGetUniformBlockIndex(handle, uniformBlock.data());
}
const void ShaderProgram::UniformBlockBinding(const std::string_view &uniformBlock) {
	glUniformBlockBinding(handle, GetUniformBlockIndex(uniformBlock), 0);
}

const GLuint ShaderProgram::CacheUniformLocation(const std::string &uniform) {
	auto hash = hash_32_fnv1a_const(uniform.c_str(), uniform.size());

	return uniforms.emplace(
		std::make_pair(
			hash,
			glGetUniformLocation(handle, uniform.c_str())
		)
	).first->second;
}

// Differing the function name (instead of just the signature)
// to make the distinction more obvious
const GLuint ShaderProgram::GetCachedUniformLocation(uint32_t hash) const noexcept {
	return uniforms.at(hash);
}

void ShaderProgram::UniformMatrix4fv(uint32_t hash, GLsizei count, GLboolean transpose, const glm::mat4 &value) const {
	glUniformMatrix4fv(
		GetCachedUniformLocation(hash),
		count,
		transpose,
		glm::value_ptr(value)
	);
}

void ShaderProgram::Uniform1i(uint32_t hash, int i) const {
	glUniform1i(
		GetCachedUniformLocation(hash),
		i
	);
}

void ShaderProgram::Uniform1f(uint32_t hash, float x) const {
	glUniform1f(
		GetCachedUniformLocation(hash),
		x
	);
}

void ShaderProgram::Uniform2f(uint32_t hash, float x, float y) const {
	glUniform2f(
		GetCachedUniformLocation(hash),
		x,
		y
	);
}

void ShaderProgram::Uniform3f(uint32_t hash, float x, float y, float z) const {
	glUniform3f(
		GetCachedUniformLocation(hash),
		x,
		y,
		z
	);
}

void ShaderProgram::Uniform4f(uint32_t hash, float x, float y, float z, float w) const {
	glUniform4f(
		GetCachedUniformLocation(hash),
		x,
		y,
		z,
		w
	);
}
}