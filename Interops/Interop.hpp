#pragma once

#include <functional>

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

	const GLuint GetFramebuffer() const { return fbo; }

protected:
	GLuint fbo = 0;
};
}