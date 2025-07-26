#pragma once

#include <map>
#include <optional>
#include <memory>

#include <glm/glm.hpp>

// Based on https://learnopengl.com/In-Practice/Text-Rendering
#include <ft2build.h>
#include FT_FREETYPE_H

#include <glad/glad.h>

#include "Buffer.hpp"
#include "Logger.hpp"
#include "ShaderProgram.hpp"
#include "Texture.hpp"
#include "VertexArray.hpp"

namespace Fetcko {
class OpenGLFont : public LoggableClass {
public:
	virtual ~OpenGLFont() = default;

	static constexpr std::string_view CharacterSet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789/:";

	struct Bounds {
		Bounds() = default;
		Bounds(int x, int y, int width, int height) :
			x(x),
			y(y),
			width(width),
			height(height) {

		}

		int x = 0, y = 0, width = 0, height = 0;
	};

	bool Init();

	void SetShader(const ShaderProgram &shader) { this->shader = shader; }

	Bounds MeasureText(const std::string &text, float scale = 1.0f) const;

	void RenderText(
		const std::string &text,
		glm::mat4 projection, // passed by VALUE
		glm::vec3 color
	);

private:
	struct Character {
		Character() = delete;
		Character(Character &&) = default;
		Character(GLint index, Texture &&texture, glm::ivec2 &&size, glm::ivec2 &&bearing, FT_Pos &&advance) :
			index(index),
			texture(std::move(texture)),
			size(std::move(size)),
			bearing(std::move(bearing)),
			advance(std::move(advance)) {

		}

		GLint index;
		Texture texture;	// ID handle of the glyph texture
		glm::ivec2 size;	// Size of glyph
		glm::ivec2 bearing;	// Offset from baseline to left/top of glyph
		FT_Pos advance;		// Offset to advance to next glyph
	};

	VertexArray vao;
	ArrayBuffer vbo;
	std::map<char, Character> characters;

	glm::vec3 lastColor;

	std::optional<std::reference_wrapper<const ShaderProgram>> shader = std::nullopt;
};
}