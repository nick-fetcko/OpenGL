#include "OpenGLFont.hpp"

#include <freetype/ftbitmap.h>

#include <glm/glm.hpp>

#include "Hash.hpp"

namespace Fetcko {
glm::vec3 OpenGLFont::lastColor = {
		std::numeric_limits<float>::infinity(),
		std::numeric_limits<float>::infinity(),
		std::numeric_limits<float>::infinity()
};

std::map<FT_ULong, OpenGLFont::Character>::iterator OpenGLFont::LoadGlyph(std::size_t i, const FT_ULong c) {
	auto face = faces.begin();

	auto index = FT_Get_Char_Index(*face, c);
	while (index == 0) {
		if (++face == faces.end()) {
			--face;
			break;
		}

		index = FT_Get_Char_Index(*face, c);
	}

	// load character glyph 
	if (FT_Load_Glyph(*face, index, FT_LOAD_DEFAULT)) {
		LogError("Failed to load Glyph");
		return characters.end();
	}

	FT_BitmapGlyph glyph = nullptr;
	{
		FT_Glyph _glyph;
		auto error = FT_Get_Glyph((*face)->glyph, &_glyph);

		if (stroker)
			error = FT_Glyph_StrokeBorder(&_glyph, stroker, false, true);

		error = FT_Glyph_To_Bitmap(&_glyph, FT_RENDER_MODE_LCD, nullptr, true);

		glyph = reinterpret_cast<FT_BitmapGlyph>(_glyph);
	}

	float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;

	// tightly pack
	std::vector<uint8_t> packed(glyph->bitmap.width * glyph->bitmap.rows);
	for (unsigned int y = 0; y < glyph->bitmap.rows; ++y) {
		for (unsigned int x = 0; x < glyph->bitmap.width; ++x) {
			packed[y * glyph->bitmap.width + x] = glyph->bitmap.buffer[
				y * glyph->bitmap.pitch + x
			];
		}
	}

	// generate texture
	Texture2D texture(GL_RED, GL_RED, true);
	texture.TexImage2D(
		glyph->bitmap.width,
		glyph->bitmap.rows,
		packed.data()
	);

	// set texture options
	texture.SetTexParameters();

	// Find max overhang below the baseline
	if (((*face)->glyph->metrics.height / 64.0 - (*face)->glyph->metrics.horiBearingY / 64.0) > overhang)
		overhang = ((*face)->glyph->metrics.height / 64.0 - (*face)->glyph->metrics.horiBearingY / 64.0);

	// now store character for later use
	const auto &ret = characters.emplace(
		c,
		Character{
			static_cast<GLint>(i),
			std::move(texture),
			glm::ivec2(glyph->bitmap.width, glyph->bitmap.rows),
			glm::ivec2(glyph->left, glyph->top),
			(*face)->glyph->metrics,
			(*face)->glyph->advance.x >> 6
		}
	).first;

	FT_Done_Glyph(reinterpret_cast<FT_Glyph>(glyph));

	const auto &ch = ret->second;

	x = static_cast<float>(ch.bearing.x);
	y = static_cast<float>(-ch.bearing.y);

	w = static_cast<float>(ch.size.x / 3);
	//w = static_cast<float>(ch.size.x);
	h = static_cast<float>(ch.size.y);

	// update VBO for each character
	float vertices[6][4] = {
		{ x,     y,     0.0f, 0.0f },
		{ x,     y + h, 0.0f, 1.0f },
		{ x + w, y + h, 1.0f, 1.0f },

		{ x,     y,     0.0f, 0.0f },
		{ x + w, y + h, 1.0f, 1.0f },
		{ x + w, y,     1.0f, 0.0f }
	};

	vbo.BufferSubData(i * 6 * 4, sizeof(vertices), vertices);

	return ret;
}

inline bool OpenGLFont::LoadInitialCharacters() {
	vao.Bind();
	vbo.Bind();
	vbo.BufferData(sizeof(float) * 6 * 4 * CharacterSet.size());
	vao.AddAttribute(VertexArray::Attribute(0, 2, 4 * sizeof(float)));
	vao.AddAttribute(VertexArray::Attribute(1, 2, 4 * sizeof(float), 2 * sizeof(float)));

	GLint previousUnpackAlignment = 0;
	glGetIntegerv(GL_UNPACK_ALIGNMENT, &previousUnpackAlignment);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // disable byte-alignment restriction

	for (const auto &[i, c] : Utils::Enumerate(CharacterSet)) {
		if (LoadGlyph(i, c) == characters.end()) {
			glPixelStorei(GL_UNPACK_ALIGNMENT, previousUnpackAlignment);
			return false;
		}
	}

	vbo.Unbind();
	vao.Unbind();

	glPixelStorei(GL_UNPACK_ALIGNMENT, previousUnpackAlignment);

	em = MeasureText("M");

	return true;
}

bool OpenGLFont::OnInit(const std::vector<std::string> &fonts, FT_UInt size, int outline) {
	if (FT_Init_FreeType(&ft)) {
		LogError("Could not init FreeType Library");
		return false;
	}

	this->fonts = fonts;

	faces.resize(fonts.size());

	SetFontSize(size);

	if (outline > 0)
		SetOutlineRadius(outline);

	if (!LoadInitialCharacters())
		return false;

	return true;
}

bool OpenGLFont::OnInit(const std::string &rootFont, FT_UInt size, int outline) {
	std::vector<std::string> fontFiles;

	// Find all fonts that start with rootFont
	for (const auto &iter : std::filesystem::directory_iterator(Utils::GetResourceFolder())) {
		if (iter.path().stem().u8string().find(rootFont) == 0 &&
			iter.path().extension().u8string() == ".ttf") {
			fontFiles.emplace_back(iter.path().filename().u8string());
		}
	}

	if (fontFiles.empty()) {
		LogError("Could not find any font files with the root of '", rootFont, "'!");
		return false;
	}

	return OnInit(fontFiles, size, outline);
}

bool OpenGLFont::SetFontSize(FT_UInt size) {
	for (auto face : faces)
		FT_Done_Face(face);

	for (const auto &[i, font] : Utils::Enumerate(fonts)) {
		auto path = Utils::GetResource(font).u8string();
		if (FT_New_Face(ft, path.c_str(), 0, &faces[i])) {
			LogError("Failed to load font ", font);
			return false;
		}

		FT_Set_Pixel_Sizes(faces[i], 0, size);
	}

	// Flush any characters we have
	characters.clear();

	return true;
}

void OpenGLFont::SetOutlineRadius(int radius) {
	FT_Stroker_Done(stroker);

	FT_Stroker_New(ft, &stroker);
	FT_Stroker_Set(stroker, radius * 64, FT_STROKER_LINECAP_ROUND, FT_STROKER_LINEJOIN_ROUND, 0);
	outlineRadius = radius;

	// Flush any characters we have
	characters.clear();
}

void OpenGLFont::OnDestroy() {
	FT_Stroker_Done(stroker);

	for (auto face : faces)
		FT_Done_Face(face);

	FT_Done_FreeType(ft);
}

std::map<FT_ULong, OpenGLFont::Character>::iterator OpenGLFont::LoadMissingGlyph(const FT_ULong c) {
	GLint previousUnpackAlignment = 0;
	glGetIntegerv(GL_UNPACK_ALIGNMENT, &previousUnpackAlignment);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // disable byte-alignment restriction

	vbo.Bind();

	// FIXME: is there a more efficient way to do this?
	auto data = vbo.GetBufferSubData(0, 6 * 4 * characters.size());
	vbo.BufferData(sizeof(float) * 6 * 4 * (characters.size() + 1));
	vbo.BufferSubData(0, 6 * 4 * characters.size() * sizeof(float), data.data());

	auto ret = LoadGlyph(characters.size(), c);

	glPixelStorei(GL_UNPACK_ALIGNMENT, previousUnpackAlignment);

	vbo.Unbind();

	return ret;
}

OpenGLFont::Bounds OpenGLFont::MeasureText(const std::string &text, float scale) {
	Bounds ret;

	ret.height = ((*faces.begin())->size->metrics.height >> 6) + outlineRadius * 2;
	ret.height -= overhang / 2;

	const auto converted = converter.from_bytes(text);

	for (const auto &c : converted) {
		// FIXME: ID3 Unicode parsing can sometimes add an additional null,
		//        so right now we have to account for that.
		if (c == '\0') break;

		auto ch = characters.find(c);
		if (ch == characters.end())
			ch = LoadMissingGlyph(c);

		ret.width += std::ceil(ch->second.advance * scale);

		// Find max y bearing
		auto y = std::ceil(ch->second.metrics.horiBearingY * scale);

		if (y > ret.y)
			ret.y = y;

		// Fid max height, taking bearing into consideration
		if (auto height = std::ceil(ch->second.metrics.height * scale); height - y > ret.renderedHeight)
			ret.renderedHeight = height - y;

		// Find max overhang below the baseline, for vertical centering
		if ((ch->second.metrics.height - ch->second.metrics.horiBearingY) * scale > ret.overhang)
			ret.overhang = (ch->second.metrics.height - ch->second.metrics.horiBearingY) * scale;
	}

	ret.y += outlineRadius;
	ret.renderedHeight += ret.y + outlineRadius;
	ret.width += outlineRadius * 2;

	return ret;
}

std::pair<std::unique_ptr<FramebufferObject>, OpenGLFont::Bounds> OpenGLFont::CacheText(const std::string &text, glm::vec3 color, Context &context) {
	const auto bounds = MeasureText(text, 1.0f);

	auto framebuffer = std::make_unique<FramebufferObject>(bounds.width, bounds.renderedHeight);
	framebuffer->SetDefaultFramebuffer(defaultFramebuffer);
	framebuffer->Bind();
	GLint oldViewport[4];

	// We don't want to apply the alpha channel twice
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

	glGetIntegerv(GL_VIEWPORT, oldViewport);
	glViewport(0, 0, bounds.width, bounds.renderedHeight);
	auto projection = glm::ortho(0.0f, static_cast<float>(bounds.width), 0.0f, static_cast<float>(bounds.renderedHeight));
	projection = glm::translate(
		projection, 
		glm::vec3(
			outlineRadius,
			bounds.y,
			0.0f
		)
	);
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	RenderText(text, projection, color, context);
	framebuffer->Unbind();
	glViewport(oldViewport[0], oldViewport[1], oldViewport[2], oldViewport[3]);

	// Return to our normal blending
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	return std::make_pair(std::move(framebuffer), bounds);
}

void OpenGLFont::RenderCached(const std::unique_ptr<FramebufferObject> &framebuffer, glm::mat4 projection, Context &context) {
	context.Translate(-outlineRadius, 0, 0);
	context.Apply();

	framebuffer->Draw(0, 0, context);

	context.Translate(outlineRadius, 0, 0);
	context.Apply();
}

void OpenGLFont::RenderText(const std::string &text, glm::mat4 projection, glm::vec3 color, Context &context) {
	context.Use("font"_hash);

	if (color != lastColor) {
		context.Color(color.x, color.y, color.z, 1.0f);
		lastColor = color;
	}

	vao.Bind();

	const auto converted = converter.from_bytes(text);

	// iterate through all characters
	for (const auto &c : converted) {
		if (c == '\0') break;

		context.GetShaderProgram().UniformMatrix4fv(
			"projection"_hash,
			1,
			GL_FALSE,
			projection
		);

		auto ch = characters.find(c);
		if (ch == characters.end())
			ch = LoadMissingGlyph(c);

		// render glyph texture over quad
		ch->second.texture.Bind();
		// render quad
		vbo.DrawArrays(GL_TRIANGLES, 6 * ch->second.index, 6);

		// now advance cursors for next glyph
		projection = glm::translate(
			projection,
			glm::vec3(
				ch->second.advance,
				0,
				0
			)
		);
	}
	vao.Unbind();

	context.Use("texture"_hash);
}
}
