#pragma once

#include <lodepng.h>

#include <glad/glad.h>

#include "Logger.hpp"

#include "Buffer.hpp"
#include "Context.hpp"
#include "Texture.hpp"
#include "VertexArray.hpp"

#define VALIDATE 0

namespace Fetcko {

#pragma pack (push, 1)
struct [[gnu::packed]] BMPHeader {
	char header[2] = { 'B', 'M' };
	uint32_t size = 0;
	uint8_t reserved[2] = { 0, 0 };
	uint8_t reserved2[2] = { 0, 0 };
	uint32_t offset = 0;
};

struct [[gnu::packed]] BITMAPINFOHEADER {
	uint32_t size = 40;
	uint32_t width = 0;
	uint32_t height = 0;
	uint16_t planes = 1;
	uint16_t bpp = 24;
	uint32_t compression = 0;
	uint32_t imageSize = 0; // a dummy 0 can be given for BI_RGB bitmaps
	uint32_t hDpi = 72;
	uint32_t vDpi = 72;
	uint32_t nColors = 0;
	uint32_t nImportant = 0;
};
#pragma pack (pop)

template<bool Multisampled>
class Framebuffer : public LoggableClass {
public:
	Framebuffer(GLsizei width, GLsizei height, GLint format = GL_RGBA) : texture(format) {
		this->width = width;
		this->height = height;

		glGenFramebuffers(1, &handle);

		// Can't call Bind() for this, as it will
		// try to bind the multisampled handle
		glBindFramebuffer(GL_FRAMEBUFFER, handle);

		texture.Bind();
		texture.TexImage2D(width, height, nullptr);
		texture.SetTexParameters();
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture.GetHandle(), 0);
		texture.Unbind();

#if VALIDATE
		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
			LogError("Framebuffer not complete!");
#endif

		if constexpr (Multisampled) {
			glGenFramebuffers(1, &multisampledHandle);

			Bind();

			multisampledTexture = std::make_unique<MultisampledTexture2D>(format);
			multisampledTexture->Bind();
			multisampledTexture->TexImage2DMultisample(width, height);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, multisampledTexture->GetHandle(), 0);
			multisampledTexture->Unbind();

			// Generate the depth attachment
			glGenRenderbuffers(1, &depthBuffer);
			glBindRenderbuffer(GL_RENDERBUFFER, depthBuffer);
			glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_DEPTH_COMPONENT, width, height);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthBuffer);

#if VALIDATE
			if (auto status = glCheckFramebufferStatus(GL_FRAMEBUFFER); status != GL_FRAMEBUFFER_COMPLETE)
				LogError("Multisampled framebuffer not complete! status = ", status);
#endif
		}

		Unbind();

		std::vector<float> squareBuffer = {
			0                          , 0                           , Buffers::TexCoordBuffer[0], Buffers::TexCoordBuffer[1],
			0                          , static_cast<GLfloat>(height), Buffers::TexCoordBuffer[2], Buffers::TexCoordBuffer[3],
			static_cast<GLfloat>(width), static_cast<GLfloat>(height), Buffers::TexCoordBuffer[4], Buffers::TexCoordBuffer[5],
			static_cast<GLfloat>(width), 0                           , Buffers::TexCoordBuffer[6], Buffers::TexCoordBuffer[7]
		};

		vao.Bind();
		vbo.Bind();
		vao.AddAttribute(VertexArray::Attribute(0, 2, 4 * sizeof(float)));
		vao.AddAttribute(VertexArray::Attribute(1, 2, 4 * sizeof(float), 2 * sizeof(float)));
		vbo.BufferData(squareBuffer);
		vbo.Unbind();
		vao.Unbind();

		eab.Bind();
		eab.BufferData<std::size(Buffers::SquareBuffer)>(Buffers::SquareBuffer);
		eab.Unbind();
	}

	Framebuffer(Framebuffer &&other) noexcept {
		handle = other.handle;
		other.handle = 0;

		width = other.width;
		height = other.height;

		texture = std::move(other.texture);
		vao = std::move(other.vao);
		vbo = std::move(other.vbo);
		eab = std::move(other.eab);

		multisampledHandle = other.multisampledHandle;
		other.multisampledHandle = 0;

		multisampledTexture = std::move(other.multisampledTexture);
		depthBuffer = other.depthBuffer;
		other.depthBuffer = 0;
	}

	virtual ~Framebuffer() {
		glDeleteFramebuffers(1, &handle);

		if constexpr (Multisampled) {
			glDeleteFramebuffers(1, &multisampledHandle);
			glDeleteRenderbuffers(1, &depthBuffer);
		}
	}

	void SetDefaultFramebuffer(GLuint defaultFramebuffer) {
		this->defaultFramebuffer = defaultFramebuffer;
	}

	void Bind() {
		if constexpr (Multisampled)
			glBindFramebuffer(GL_FRAMEBUFFER, multisampledHandle);
		else
			glBindFramebuffer(GL_FRAMEBUFFER, handle);
	}

	void Unbind() {
		glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebuffer);
	}

	template<
		bool _Multisampled = Multisampled,
		typename std::enable_if_t<_Multisampled == true, bool> * = nullptr
	>
	void DrawMultisampled(GLfloat x, GLfloat y, Context &context) {
		multisampledTexture->Bind();

		_Draw(x, y, context);

		multisampledTexture->Unbind();
	}

	inline void Blit(Framebuffer *target = nullptr) {
		glBindFramebuffer(GL_READ_FRAMEBUFFER, multisampledHandle);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, target ? target->multisampledHandle : handle);

		glBlitFramebuffer(
			0, height, width, 0, // invert Y so we aren't upside-down
			0, 0, width, height,
			GL_COLOR_BUFFER_BIT,
			GL_LINEAR
		);

		Unbind();
	}

	void Draw(GLfloat x, GLfloat y, Context &context, Framebuffer *target = nullptr) {
		if constexpr (Multisampled) {
			Blit(target);

			if (target)
				return;
		}

		texture.Bind();

		_Draw(x, y, context);

		texture.Unbind();
	}

	void DrawWithoutBlitting(GLfloat x, GLfloat y, Context &context) {
		texture.Bind();

		_Draw(x, y, context);

		texture.Unbind();
	}

	void Save(const std::filesystem::path &path) {
		auto rgb = GetBitmap(GL_RGB);
		std::ofstream outFile(path, std::ios::binary | std::ios::out);
		BMPHeader header;
		header.offset = sizeof(BMPHeader) + sizeof(BITMAPINFOHEADER);
		header.size = width * height * 3 + header.offset;

		BITMAPINFOHEADER infoHeader;
		infoHeader.width = width;
		infoHeader.height = height;
		
		outFile.write(reinterpret_cast<const char *>(&header), sizeof(BMPHeader));
		outFile.write(reinterpret_cast<const char *>(&infoHeader), sizeof(BITMAPINFOHEADER));
		outFile.write(reinterpret_cast<const char *>(rgb.data()), static_cast<GLsizei>(std::ceil(width * 3 / 4.0f) * 4) * height);
		outFile.close();
	}

	void SaveAsPNG(const std::filesystem::path &path) {
		auto pixels = GetBitmap();
		lodepng::encode(path.u8string(), pixels, width, height);
	}

	const GLsizei &GetWidth() const { return width; }
	const GLsizei &GetHeight() const { return height; }

	std::vector<uint8_t> GetBitmap(GLenum format = GL_RGBA, bool upsideDown = true) const {
		texture.Bind();

		auto pitch = static_cast<GLsizei>(std::ceil(width * 3 / 4.0f) * 4);

		std::vector<uint8_t> rgba(width * height * 4);
		std::vector<uint8_t> rgb;

		glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());

		if (format == GL_RGB) {
			rgb.resize(pitch * height);
			memset(rgb.data(), 0, pitch * height);
			for (GLsizei y = 0; y < height; ++y) {
				for (GLsizei x = 0; x < width; ++x) {
					memcpy(rgb.data() + (y * pitch) + (x * 3), rgba.data() + (y * width + x) * 4, 3);
				}
			}
		}

		// We need to flip vertically, since the texture is loaded upside-down
		if (upsideDown) {
			std::vector<uint8_t> flipped(height * (format == GL_RGBA ? width * 4 : pitch));

			for (GLsizei y = height - 1; ; --y) {
				memcpy(
					flipped.data() + (height - 1 - y) * (format == GL_RGBA ? width * 4 : pitch),
					&(format == GL_RGBA ? rgba : rgb).data()[y * (format == GL_RGBA ? width * 4 : pitch)],
					(format == GL_RGBA ? width * 4 : pitch)
				);

				if (y == 0)
					break;
			}

			return flipped;
		}

		return format == GL_RGBA ? rgba : rgb;
	}

protected:
	inline void _Draw(GLfloat x, GLfloat y, Context &context) {
		context.Translate(x, y, 0);
		context.Apply();

		vao.Bind();
		eab.Bind();
		eab.DrawElements(GL_TRIANGLES);
		eab.Unbind();
		vao.Unbind();
	}

	GLuint handle = 0;

	GLsizei width = 0;
	GLsizei height = 0;

	Texture2D texture;

	VertexArray vao;
	ArrayBuffer vbo;
	ElementBuffer eab;

	GLuint multisampledHandle = 0;
	std::unique_ptr<MultisampledTexture2D> multisampledTexture;
	GLuint depthBuffer = 0;

	GLuint defaultFramebuffer = 0;
};

using FramebufferObject = Framebuffer<false>;
using MultisampledFramebufferObject = Framebuffer<true>;

}