#ifdef WIN32
#pragma once

#include <glad/glad.h>

#include <dxgi1_6.h>
#include <d3d11.h>

#pragma comment (lib, "dxgi.lib")
#pragma comment (lib, "d3d11.lib")

#include "Logger.hpp"

namespace Fetcko {
// Derived from https://gist.github.com/mmozeiko/c99f9891ce723234854f0919bfd88eae
class DXGI : public LoggableClass {
public:
	bool OnInit(HWND hwnd, int adapterIndex, int width, int height);
	bool OnResize(int width, int height);
	void OnDestroy();
	bool OnLoop();
	void SwapBuffers();

	void Bind();

	const GLuint GetFramebuffer() const { return fbuf; }

private:
	ID3D11DeviceContext *deviceContext = nullptr;
	IDXGISwapChain1 *swapChain = nullptr;
	GLuint colorRbuf = 0;
	GLuint dsRbuf = 0;
	GLuint fbuf = 0;
	ID3D11RenderTargetView *colorView = nullptr;
	ID3D11DepthStencilView *dsView = nullptr;
	HANDLE dxColor = nullptr, dxDepthStencil = nullptr;
	HANDLE dxDevice = nullptr;
	HANDLE dxObjects[2] = { nullptr };
	ID3D11Device *device = nullptr;
};
}
#endif