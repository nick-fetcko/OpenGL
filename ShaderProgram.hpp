#pragma once

#include <functional>
#include <optional>

#include <glm/gtc/type_ptr.hpp>

#include "Shader.hpp"

namespace Fetcko {
class ShaderProgram : public LoggableClass {
public:
	ShaderProgram();
	ShaderProgram(ShaderProgram &&other) noexcept;

	virtual ~ShaderProgram();

	void Attach(
		const VertexShader &vertexShader,
		const std::vector<FragmentShader> &fragmentShaders
	);

	void Use() const;

	const GLuint &GetHandle() const;
	const GLuint GetUniformBlockIndex(const std::string_view &uniformBlock) const;
	const void UniformBlockBinding(const std::string_view &uniformBlock);

	template<bool Cached>
	const GLuint GetUniformLocation(const std::string &uniform) {
		if constexpr (Cached) {
			if (auto iter = uniforms.find(uniform); iter != uniforms.end())
				return iter->second;

			return CacheUniformLocation(uniform);
		}

		return glGetUniformLocation(handle, uniform.c_str());
	}

	const GLuint CacheUniformLocation(const std::string &uniform);

	// Differing the function name (instead of just the signature)
	// to make the distinction more obvious
	const GLuint GetCachedUniformLocation(const std::string &uniform) const noexcept;

	void UniformMatrix4fv(const std::string &uniform, GLsizei count, GLboolean transpose, const glm::mat4 &value) const;

	void Uniform1f(const std::string &uniform, float x);
	void Uniform2f(const std::string &uniform, float x, float y);
	void Uniform3f(const std::string &uniform, float x, float y, float z) const;
	void Uniform4f(const std::string &uniform, float x, float y, float z, float w) const;

private:
	GLuint handle = 0;

	std::optional<std::reference_wrapper<const VertexShader>> vertexShader = std::nullopt;
	std::optional<std::reference_wrapper<const std::vector<FragmentShader>>> fragmentShaders = std::nullopt;

	std::map<std::string, GLuint> uniforms;
};
}