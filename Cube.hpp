#pragma once

#include <filesystem>
#include <thread>

#include <glad/glad.h>

#include "MathCPP/Colour.hpp"

#include "Texture.hpp"
#include "Utils.hpp"

using namespace MathsCPP;

namespace Fetcko {
class Cube : public LoggableClass {
public:
	Cube() : texture(
#ifndef __ANDROID__
		GL_RGB, GL_RGB
#else
		GL_RGB16F, GL_RGB
#endif
	) {

	}
	Cube(const std::filesystem::path &path) : texture(
#ifndef __ANDROID__
			GL_RGB, GL_RGB
#else
			GL_RGB16F, GL_RGB
#endif
	) {
		Load(path);
	}

	~Cube() {
		if (loadThread.joinable())
			loadThread.join();
	}

	void Load(const std::filesystem::path &path) {
		this->path = path;

		loadThread = std::thread([&] {
			std::unique_lock lock(mutex);
			std::ifstream inFile(this->path, std::ios::in);

			std::string line;
			std::size_t index = 0;
			while (std::getline(inFile, line)) {
				auto split = Utils::Split(line, std::isspace);
				if (split.size() == 2 && split[0] == "LUT_3D_SIZE") {
					tableDimensionSize = std::stoi(split[1]);
					table = new float[static_cast<std::size_t>(std::pow(tableDimensionSize, 3) * 3)];
				}
				if (split.size() == 3 && split[0][0] != '#' && table) {
					table[index * 3] = std::stof(split[0]);
					table[index * 3 + 1] = std::stof(split[1]);
					table[index * 3 + 2] = std::stof(split[2]);

					++index;
				}
			}
		});
	}

	void OnLoop() {
		if (table && mutex.try_lock()) {
			if (loadThread.joinable())
				loadThread.join();

			texture.Bind();

			texture.TexImage3D(
				tableDimensionSize,
				tableDimensionSize,
				tableDimensionSize,
				table,
				GL_FLOAT
			);

			glTexParameterf(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameterf(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameterf(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameterf(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

			texture.Unbind();

			delete[] table;
			table = nullptr;

			LogDebug("Loaded .cube of size ", tableDimensionSize, " from ", path);

			mutex.unlock();
		}
	}

	void Bind() const { texture.Bind(); }
	void Unbind() const { texture.Unbind(); }

private:
	Texture3D texture;

	std::thread loadThread;
	std::mutex mutex;
	std::size_t tableDimensionSize = 0;
	float *table = nullptr;

	std::filesystem::path path;
};
}