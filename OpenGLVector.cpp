#include "OpenGLVector.hpp"

namespace Fetcko {
OpenGLVector::~OpenGLVector() {
}

bool OpenGLVector::Load(const std::filesystem::path &path, float size) {
	return Load(path, { size, -1.0f });
}

bool OpenGLVector::Load(const std::filesystem::path &path, Size<float> size) {
	auto string = Utils::GetStringFromFile(path);

	if (string.empty()) return false;

	auto stream = std::stringstream(string);
	auto min = Size<float>::Max();
	auto max = Size<float>::Min();

	std::string line;
	try {
		while (std::getline(stream, line)) {
			auto split = Utils::Split(line, ' ');

			if (split[0] == "v") /* v for vertex */ {
				// For 3D objects
				if constexpr (false) { // TODO: Allow for selection of 3D vs. 2D
					vertices.emplace_back(std::stof(split[1]));
					vertices.emplace_back(std::stof(split[2]));
					vertices.emplace_back(std::stof(split[3]));
				}

				// For 2D objects, we treat z as our y axis
				auto x = std::stof(split[1]);
				auto y = std::stof(split[3]) * -1.0f; // Rotate by 180 degrees, since y starts at the _bottom_ of the screen

				if (x > max.width) max.width = x;
				if (x < min.width) min.width = x;
				if (y > max.height) max.height = y;
				if (y < min.height) min.height = y;

				vertices.emplace_back(x);
				vertices.emplace_back(y);

				// Bogus texture coords (for now)
				//vertices.emplace_back(0.0f);
				//vertices.emplace_back(1.0f);

			} else if (split[0] == "f") /* f for face */ {
				// https://en.wikipedia.org/wiki/Wavefront_.obj_file#Face_elements
				// Faces start at 1, so we need to make them 0-indexed
				indices.emplace_back(std::stoi(split[1]) - 1);
				indices.emplace_back(std::stoi(split[2]) - 1);
				indices.emplace_back(std::stoi(split[3]) - 1);
			}
		}
	}
	catch (std::exception &e) {
		LogError("Could not load OBJ file due to ", e.what());
		return false;
	}

	LogDebug("Bounds = {", min, ", ", max, "}");

	// Find the absolute value of
	// the lowest _negative_ values
	auto lowestNegative = Size(
		std::abs(
			std::min(
				0.0f,
				std::min(
					min.width,
					max.width
				)
			)
		),
		std::abs(
			std::min(
				0.0f,
				std::min(
					min.height,
					max.height
				)
			)
		)
	);

	// Shift to get rid of negatives since
	// the assumption is that we're in an
	// orthographic projection with an origin
	// at {0, 0}
	min += lowestNegative;
	max += lowestNegative;

	// Find the lowest _positive_ value
	auto lowestPositive = Size(
		std::min(
			min.width,
			max.width
		),
		std::min(
			min.height,
			max.height
		)
	);

	// We want our origin to be as close
	// to {0, 0} as possible
	min -= lowestPositive;
	max -= lowestPositive;

	// If our height is negative, we should adapt to aspect ratio
	if (size.height < 0) {
		if (max.width > max.height) {
			size.height = size.width * (max.height / max.width);
		} else {
			size.height = size.width;
			size.width = size.height * (max.width / max.height);
		}
	}

	// Convert range to {0, size}
	for (std::size_t i = 0; i < vertices.size(); i += 2) {
		vertices[i] = ((vertices[i] + lowestNegative.width - lowestPositive.width) / max.width) * size.width;
		vertices[i + 1] = ((vertices[i + 1] + lowestNegative.height - lowestPositive.height) / max.height) * size.height;
	}

	this->size = size;

	vao.Bind();
	vbo.Bind();
	vbo.BufferData(vertices);
	vao.AddAttribute(VertexArray::Attribute(0, 2));
	vbo.Unbind();
	vao.Unbind();

	indexBuffer.Bind();
	indexBuffer.BufferData(indices);

	return true;
}

void OpenGLVector::Render() {
	glDisable(GL_BLEND);
	vao.Bind();
	indexBuffer.Bind();
	indexBuffer.DrawElements(GL_TRIANGLES);
	vao.Unbind();
}
}