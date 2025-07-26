#include "OpenGLFont.hpp"

namespace Fetcko {
bool OpenGLFont::Init() {
	FT_Library ft;
	if (FT_Init_FreeType(&ft)) {
		logger.LogError("Could not init FreeType Library");
		return false;
	}

	FT_Face face;
	auto path = Utils::GetResource("Roboto-Regular.ttf").u8string();
	if (FT_New_Face(ft, path.c_str(), 0, &face)) {
		logger.LogError("Failed to load font");
		return false;
	}

	FT_Set_Pixel_Sizes(face, 0, 48);

	vao.Bind();
	vbo.Bind();
	vbo.BufferData(sizeof(float) * 6 * 4 * CharacterSet.size());
	vao.AddAttribute(VertexArray::Attribute(0, 2, 4 * sizeof(float)));
	vao.AddAttribute(VertexArray::Attribute(1, 2, 4 * sizeof(float), 2 * sizeof(float)));

	GLint previousUnpackAlignment = 0;
	glGetIntegerv(GL_UNPACK_ALIGNMENT, &previousUnpackAlignment);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // disable byte-alignment restriction

	// Initialize loop variables
	float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;

	for (const auto &[i, c] : Utils::Enumerate(CharacterSet)) {
		// load character glyph 
		if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
			logger.LogError("Failed to load Glyph");
			glPixelStorei(GL_UNPACK_ALIGNMENT, previousUnpackAlignment);
			return false;
		}

		// generate texture
		Texture texture(GL_RED, GL_RED, true);
		texture.TexImage2D(
			face->glyph->bitmap.width,
			face->glyph->bitmap.rows,
			face->glyph->bitmap.buffer
		);

		// set texture options
		texture.SetTexParameters();

		// now store character for later use
		const auto &ch = characters.emplace(
			c,
			Character {
				static_cast<GLint>(i),
				std::move(texture),
				glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
				glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
				face->glyph->advance.x >> 6
			}
		).first->second;

		x = static_cast<float>(ch.bearing.x);
		y = static_cast<float>(-(ch.size.y - ch.bearing.y));

		w = static_cast<float>(ch.size.x);
		h = static_cast<float>(ch.size.y);
		
		// update VBO for each character
		float vertices[6][4] = {
			{ x,     y + h,   0.0f, 0.0f },
			{ x,     y,       0.0f, 1.0f },
			{ x + w, y,       1.0f, 1.0f },

			{ x,     y + h,   0.0f, 0.0f },
			{ x + w, y,       1.0f, 1.0f },
			{ x + w, y + h,   1.0f, 0.0f }
		};

		vbo.BufferSubData(i * sizeof(vertices), sizeof(vertices), vertices);
	}

	vbo.Unbind();
	vao.Unbind();

	glPixelStorei(GL_UNPACK_ALIGNMENT, previousUnpackAlignment);

	FT_Done_Face(face);
	FT_Done_FreeType(ft);

	return true;
}

OpenGLFont::Bounds OpenGLFont::MeasureText(const std::string &text, float scale) const {
	Bounds ret;

	for (const auto &c : text) {
		const Character &ch = characters.at(c);
		ret.width += std::lround(ch.advance * scale);

		// Find max bearing
		if (ch.bearing.y * scale > ret.height)
			ret.height = std::lround(ch.bearing.y * scale);
	}

	return ret;
}

void OpenGLFont::RenderText(const std::string &text, glm::mat4 projection, glm::vec3 color) {
	// Only update our uniform if it needs updating
	if (color != lastColor && shader) {
		shader->get().Uniform3f("color", color.x, color.y, color.z);
		lastColor = color;
	}

	glEnable(GL_BLEND);
	glActiveTexture(GL_TEXTURE0);
	vao.Bind();

	// iterate through all characters
	for (const auto &c : text) {
		if (shader) {
			shader->get().UniformMatrix4fv(
				"projection",
				1,
				GL_FALSE,
				projection
			);
		}

		const Character &ch = characters.at(c);

		// render glyph texture over quad
		ch.texture.Bind();
		// render quad
		vbo.DrawArrays(GL_TRIANGLES, 6 * ch.index, 6);

		// now advance cursors for next glyph
		projection = glm::translate(
			projection,
			glm::vec3(
				ch.advance,
				0,
				0
			)
		);
	}
	vao.Unbind();
}
}