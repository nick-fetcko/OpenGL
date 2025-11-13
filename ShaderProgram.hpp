#pragma once

#include <functional>
#include <optional>

#include <glm/gtc/type_ptr.hpp>

#include "Hash.hpp"

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
	constexpr const GLuint GetUniformLocation(const std::string &uniform) {
		if constexpr (Cached) {
			const auto hash = hash_32_fnv1a_const(uniform.c_str(), uniform.size());

			if (auto iter = uniforms.find(hash); iter != uniforms.end())
				return iter->second;

			return CacheUniformLocation(uniform);
		}

		return glGetUniformLocation(handle, uniform.c_str());
	}

	const GLuint CacheUniformLocation(const std::string &uniform);

	// Differing the function name (instead of just the signature)
	// to make the distinction more obvious
	const GLuint GetCachedUniformLocation(uint32_t hash) const noexcept;

	void UniformMatrix4fv(uint32_t hash, GLsizei count, GLboolean transpose, const glm::mat4 &value) const;

	void Uniform1i(uint32_t hash, int i) const;
	void Uniform1f(uint32_t hash, float x) const;
	void Uniform2f(uint32_t hash, float x, float y) const;
	void Uniform3f(uint32_t hash, float x, float y, float z) const;
	void Uniform4f(uint32_t hash, float x, float y, float z, float w) const;

private:
	GLuint handle = 0;

	std::optional<std::reference_wrapper<const VertexShader>> vertexShader = std::nullopt;
	std::optional<std::reference_wrapper<const std::vector<FragmentShader>>> fragmentShaders = std::nullopt;

	std::map<uint32_t, GLuint> uniforms;
};
}