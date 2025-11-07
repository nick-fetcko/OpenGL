#pragma once

#include <filesystem>
#include <vector>

#include "Hash.hpp"

#include "Logger.hpp"
#include "Shader.hpp"
#include "ShaderProgram.hpp"

namespace Fetcko {
class Context {
public:
	struct Shader {
		VertexShader vertex;
		std::vector<FragmentShader> fragments;
		ShaderProgram program;
	};
	//VertexShader &GetVertexShader() { return vertexShader; }
	//FragmentShader &GetFragmentShader() { return fragmentShader; }

	ShaderProgram &GetShaderProgram() { return currentShader->program; }

	Shader *AddShader(
		std::filesystem::path vertex,
		std::filesystem::path fragment,
		std::uint32_t hash
	) {
		return AddShader(vertex, std::vector<std::filesystem::path>{ fragment }, hash);
	}

	Shader *AddShader(
		std::filesystem::path vertex,
		std::vector<std::filesystem::path> fragments,
		std::uint32_t hash
	) {
		Shader shader;

		shader.vertex.Compile(vertex);

		for (const auto &fragment : fragments) {
			FragmentShader fragmentShader;
			fragmentShader.Compile(fragment);
			shader.fragments.emplace_back(std::move(fragmentShader));
		}

		shader.program.Attach(
			shader.vertex,
			shader.fragments
		);

		auto program = &shaders.emplace(std::make_pair(hash, std::move(shader))).first->second;

		// Assume the first shader should be the current one
		if (!currentShader) currentShader = program;

		return program;
	}

	const Shader *GetShader(std::uint32_t hash) const {
		return &shaders.at(hash);
	}

	void RemoveShader(std::uint32_t hash) {
		shaders.erase(hash);
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

	const glm::mat4 &GetIdentity() const { return identity; }

	void SetIdentity(glm::mat4 &&projection) { identity = std::move(projection); this->projection = identity; }
	void SetProjection(glm::mat4 &&projection) { this->projection = std::move(projection); }

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
		currentShader->program.Uniform4f("color"_hash, r, g, b, a);
	}

	inline void Apply() {
		currentShader->program.UniformMatrix4fv("projection"_hash, 1, GL_FALSE, projection);
	}

	inline void LoadIdentity() { 
		projection = identity;
		Apply();
	}

	const float &GetYOffset() const { return yOffset; }
	void SetYOffset(float yOffset) { this->yOffset = yOffset; }

private:
	std::map<std::uint32_t, Shader> shaders;
	Shader *currentShader = nullptr;

	glm::mat4 identity{ 0.0f };
	glm::mat4 projection{ 0.0f };

	float yOffset = 0.0f;
};
}
