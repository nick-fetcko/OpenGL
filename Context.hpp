#pragma once

#include <filesystem>
#include <vector>

#include "MathCPP/Rectangle.hpp"

#include "Hash.hpp"

#include "Logger.hpp"
#include "Shader.hpp"
#include "ShaderProgram.hpp"

#define VALIDATE_THREAD 0

using namespace MathsCPP;

namespace Fetcko {
class Context 
#if VALIDATE_THREAD
	: public LoggableClass
#endif
{
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

#if VALIDATE_THREAD
		if (!mainThread)
			mainThread = std::this_thread::get_id();
#endif

		return AddShader(vertex, std::vector<std::filesystem::path>{ fragment }, hash);
	}

	Shader *AddShader(
		std::filesystem::path vertex,
		std::vector<std::filesystem::path> fragments,
		std::uint32_t hash
	) {
		Shader shader;

		shader.vertex.Compile(vertex);

#ifndef __ANDROID__
		for (const auto &fragment : fragments) {
			FragmentShader fragmentShader;
			fragmentShader.Compile(fragment);
			shader.fragments.emplace_back(std::move(fragmentShader));
		}
#else
		std::string concat;
		for (const auto &fragment : fragments) {
			concat += Utils::GetStringFromFile(fragment);
		}

		FragmentShader fragmentShader;
		fragmentShader.Compile(concat);
		shader.fragments.emplace_back(std::move(fragmentShader));
#endif

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

#if VALIDATE_THREAD
		if (mainThread && std::this_thread::get_id() != *mainThread)
			LogWarning("Using shader on wrong thread!");
#endif

		currentShaderHash = hash;
	}

	std::uint32_t GetLastHashAndUse(std::uint32_t hash) {
		const auto ret = currentShaderHash;

		Use(hash);

		return ret;
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

	void SetSafeArea(Rectanglei &&rect) { this->safeArea = std::move(rect); }
	const Rectanglei &GetSafeArea() const { return safeArea; }

	// Blends while maintaining the _destination_'s alpha
	inline void Blend(bool enabled, std::function<void()> &&f) {
		if (enabled) StartBlend();

		f();

		if (enabled) EndBlend();
	}

	inline void StartBlend() {
		glBlendFuncSeparate(
			GL_SRC_ALPHA,
			GL_ONE_MINUS_SRC_ALPHA,
			GL_ZERO,
			GL_ONE
		);
	}

	inline void EndBlend() {
		glBlendFuncSeparate(
			GL_SRC_ALPHA,
			GL_ONE_MINUS_SRC_ALPHA,
			GL_SRC_ALPHA,
			GL_ONE_MINUS_SRC_ALPHA
		);
	}

private:
	std::map<std::uint32_t, Shader> shaders;
	std::uint32_t currentShaderHash = 0;
	Shader *currentShader = nullptr;

	glm::mat4 identity{ 0.0f };
	glm::mat4 projection{ 0.0f };

	float yOffset = 0.0f;

	Rectanglei safeArea{0, 0, 0, 0};

#if VALIDATE_THREAD
	std::optional<std::thread::id> mainThread = std::nullopt;
#endif
};
}
