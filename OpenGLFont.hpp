#pragma once

#include <codecvt>
#include <map>
#include <optional>
#include <locale>
#include <memory>

#include <freetype/ftstroke.h>

#include <glm/glm.hpp>

// Based on https://learnopengl.com/In-Practice/Text-Rendering
#include <ft2build.h>
#include FT_FREETYPE_H

#include <glad/glad.h>

#include "Buffer.hpp"
#include "Context.hpp"
#include "Framebuffer.hpp"
#include "Logger.hpp"
#include "ShaderProgram.hpp"
#include "Texture.hpp"
#include "VertexArray.hpp"

namespace Fetcko {
class OpenGLFont : public LoggableClass {
public:
	virtual ~OpenGLFont() = default;

	static constexpr std::string_view CharacterSet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789/:' ,_()-.$";

	struct Bounds {
		Bounds() = default;
		Bounds(int x, int y, int width, int height) :
			x(x),
			y(y),
			width(width),
			height(height) {

		}

		int x = 0, y = 0, width = 0, height = 0, renderedHeight = 0;
	};

	bool OnInit(const std::vector<std::string> &fonts, FT_UInt size, int outline = 0);
	bool OnInit(const std::string &rootFont, FT_UInt size, int outline = 0);

	const bool HasFaces() const { return !faces.empty(); }
	
	bool SetFontSize(FT_UInt size);
	void SetOutlineRadius(int radius);

	const int GetOutlineRadius() const { return outlineRadius; }

	void OnDestroy();

	Bounds MeasureText(const std::string &text, float scale = 1.0f);
	FT_Short GetAscender() const { return std::abs(faces.at(0)->ascender >> 6); }
	FT_Short GetDescender() const { return std::abs(faces.at(0)->size->metrics.descender >> 6); }

	std::pair<std::unique_ptr<FramebufferObject>, Bounds> CacheText(const std::string &text, glm::vec3 color, Context &context);
	void RenderText(
		const std::string &text,
		glm::mat4 projection, // passed by VALUE
		glm::vec3 color,
		Context &context
	);

	void RenderCached(const std::unique_ptr<FramebufferObject> &framebuffer, glm::mat4 projection, Context &context);

	const OpenGLFont::Bounds &GetEm() const { return em; }

private:
	struct Character {
		Character() = delete;
		Character(Character &&) = default;
		Character(GLint index, Texture2D &&texture, glm::ivec2 &&size, glm::ivec2 &&bearing, FT_Glyph_Metrics &metrics, FT_Pos &&advance) :
			index(index),
			texture(std::move(texture)),
			size(std::move(size)),
			bearing(std::move(bearing)),
			metrics(metrics),
			advance(std::move(advance)) {
			this->metrics.height >>= 6;
			this->metrics.horiBearingY >>= 6;
		}

		GLint index;
		Texture2D texture;	// ID handle of the glyph texture
		glm::ivec2 size;	// Size of glyph
		glm::ivec2 bearing;	// Offset from baseline to left/top of glyph
		FT_Glyph_Metrics metrics;
		FT_Pos advance;		// Offset to advance to next glyph
	};

	inline bool LoadInitialCharacters();
	inline std::map<FT_ULong, Character>::iterator LoadGlyph(std::size_t i, const FT_ULong c);
	std::map<FT_ULong, Character>::iterator LoadMissingGlyph(const FT_ULong c);

	std::vector<std::string> fonts;

	FT_Library ft;
	std::vector<FT_Face> faces;

	VertexArray vao;
	ArrayBuffer vbo;
	std::map<FT_ULong, Character> characters;

	// Needs to be static since multiple instances
	// of OpenGLFont could update the shader uniform
	static glm::vec3 lastColor;

	std::unique_ptr<FramebufferObject> framebuffer;

	std::wstring_convert<std::codecvt_utf8<uint32_t>, uint32_t> converter;

	FT_Stroker stroker = nullptr;

	int outlineRadius = 0;

	Bounds em{ 0, 0, 0, 0 };
};
}