#pragma once

#include <filesystem>
#include <vector>

#include "Logger.hpp"
#include "Shader.hpp"
#include "ShaderProgram.hpp"

namespace Fetcko {
class Context {
public:
	struct Shader {
		VertexShader vertex;
		FragmentShader fragment;
		ShaderProgram program;
	};
	//VertexShader &GetVertexShader() { return vertexShader; }
	//FragmentShader &GetFragmentShader() { return fragmentShader; }

	ShaderProgram &GetShaderProgram() { return currentShader->program; }

	void AddShader(
		std::filesystem::path &vertex,
		std::filesystem::path &fragment,
		std::uint32_t hash
	) {
		Shader shader;

		shader.vertex.Compile(vertex);
		shader.fragment.Compile(fragment);

		shader.program.Attach(
			shader.vertex,
			shader.fragment
		);

		auto program = &shaders.emplace(std::make_pair(hash, std::move(shader))).first->second;

		// Assume the first shader should be the current one
		if (!currentShader) currentShader = program;
	}

	std::map<std::uint32_t, Shader>::iterator begin() {
		return shaders.begin();
	}

	std::map<std::uint32_t, Shader>::iterator end() {
		return shaders.end();
	}

	void Use(std::uint32_t hash) {
		currentShader = &shaders.at(hash);
		currentShader->program.Use();
	}

	void With(std::uint32_t hash, std::function<void(Shader&)> f) {
		auto &shader = shaders.at(hash);
		shader.program.Use();

		f(shader);

		currentShader->program.Use();
	}

	void SetProjection(glm::mat4 &&projection) { identity = std::move(projection); this->projection = identity; }
	const glm::mat4 &GetProjection() const { return projection; }

	inline void Translate(float x, float y, float z) { 
		projection = glm::translate(
			projection, 
			glm::vec3(x, y, z)
		); 
	}
	inline void Rotate(float angle, float x, float y, float z) { 
		projection = glm::rotate(
			projection,
			glm::radians(angle),
			glm::vec3(x, y, z)
		); 
	}
	inline void Scale(float x, float y, float z) {
		projection = glm::scale(
			projection,
			glm::vec3(x, y, z)
		);
	}
	inline void Color(float r, float g, float b, float a) {
		currentShader->program.Uniform4f("color", r, g, b, a);
	}

	inline void Apply() {
		currentShader->program.UniformMatrix4fv("projection", 1, GL_FALSE, projection);
	}

	inline void LoadIdentity() { 
		projection = identity; 
		Apply();
	}

private:
	std::map<std::uint32_t, Shader> shaders;
	Shader *currentShader = nullptr;

	glm::mat4 identity;
	glm::mat4 projection;
};
}