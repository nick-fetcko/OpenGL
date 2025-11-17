#ifdef WIN32
#pragma once

#include <optional>
#include <tuple>

#include <glad/glad.h>

#include <dxgi1_6.h>
#include <d3d11.h>

#pragma comment (lib, "dxgi.lib")
#pragma comment (lib, "d3d11.lib")

#include "Interop.hpp"

#include "Logger.hpp"

namespace Fetcko {
// Derived from https://gist.github.com/mmozeiko/c99f9891ce723234854f0919bfd88eae
class DXGI : public Interop, public LoggableClass {
public:
	DXGI(bool depthBuffer = true);
	virtual ~DXGI();

	bool OnInit(const InitArgs &args) override;
	bool OnCreate(HWND hwnd, int width, int height);
	bool OnResize(int width, int height) override;
	void OnDestroy() override;
	bool OnLoop() override;
	bool SwapBuffers() override;

	void Bind();

	IDXGISwapChain1 *GetSwapChain() { return swapChain; }

	std::optional<std::tuple<bool, float, float>> GetHdrProperties(int outputIndex);

private:
	inline bool Load();
	inline void Unload();

	bool depthBuffer = true;

	IDXGIFactory6 *factory = nullptr;
	IDXGIAdapter1 *adapter = nullptr;
	IDXGIOutput *output = nullptr;

	ID3D11DeviceContext *deviceContext = nullptr;
	IDXGISwapChain1 *swapChain = nullptr;
	GLuint colorRbuf = 0;
	GLuint dsRbuf = 0;
	
	ID3D11RenderTargetView *colorView = nullptr;
	ID3D11DepthStencilView *dsView = nullptr;
	HANDLE dxDevice = nullptr;
	HANDLE dxObjects[2] = { nullptr };
	ID3D11Device *device = nullptr;

	ID3D11Texture2D *colorBuffer = nullptr;
	ID3D11Texture2D *dsBuffer = nullptr;
	HRESULT hr = S_OK;

	int lastOutputIndex = -1;
};
}
#endif