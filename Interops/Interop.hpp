#pragma once

#include <functional>

#include <glad/glad.h>

namespace Fetcko {
class Interop {
public:
	struct InitArgs {
		int adapterIndex; // DXGI
		int width; // Vulkan
		int height; // Vulkan
		std::function<void *(void *)> surfaceCallback; // Vulkan
	};

	virtual bool OnInit(const InitArgs &args) = 0;
	virtual bool OnResize(int width, int height) = 0;

	virtual void OnDestroy() = 0;

	virtual bool OnLoop() = 0;
	virtual bool SwapBuffers() = 0;

	virtual void SetHdr(bool enabled, void *hwnd = nullptr, int width = 0, int height = 0) = 0;

	const GLuint GetFramebuffer() const { return fbo; }

protected:
	GLuint fbo = 0;
};
}