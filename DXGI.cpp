#ifdef WIN32
#include "DXGI.hpp"

#include <glad/glad_wgl.h>

namespace Fetcko {
DXGI::DXGI(bool depthBuffer) :
	depthBuffer(depthBuffer){

}

bool DXGI::OnInit(HWND hwnd, int adapterIndex, int width, int height) {
	IDXGIFactory6 *factory = nullptr;
	bool success = false;
	IDXGIAdapter1 *adapter = nullptr;
	HRESULT hr = S_OK;

	if (hr = CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, __uuidof(IDXGIFactory6), (void **)&factory); hr != S_OK) {
		LogError("Could not create DXGIFactory2!");
		return false;
	}

	if (hr = factory->EnumAdapters1(adapterIndex, &adapter); hr != S_OK) {
		LogError("IDXGIFactory::EnumAdapters1() failed: ", std::hex, hr);
		return false;
	}

	DXGI_ADAPTER_DESC1 adapterDesc;

	if (hr = adapter->GetDesc1(&adapterDesc); hr != S_OK) {
		LogError("IDXGIAdapter::GetDesc() failed: ", std::hex, hr);
		return false;
	}

	if (adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
		LogError("Got software DXGI adapter");
		return false;
	}

	IDXGIOutput *output = nullptr;

	if (hr = adapter->EnumOutputs(0, &output); hr != S_OK) {
		LogError("IDXGIAdapter::EnumOutputs() failed: ", std::hex, hr);
		return false;
	}

	IDXGIOutput6 *output6 = nullptr;
	
	if (hr = output->QueryInterface(__uuidof(IDXGIOutput6), (void **)&output6); hr == S_OK) {
		// Use output6...
		DXGI_OUTPUT_DESC1 desc;
		output6->GetDesc1(&desc);
		output6->Release();
	} else {
		LogError("Could not query output interface: ", std::hex, hr);
		return false;
	}

	hr = D3D11CreateDevice(
		adapter,
		D3D_DRIVER_TYPE_UNKNOWN,
		nullptr,
#ifdef _DEBUG
		D3D11_CREATE_DEVICE_DEBUG |
#endif
		D3D11_CREATE_DEVICE_BGRA_SUPPORT,
		nullptr,
		0,
		D3D11_SDK_VERSION,
		&device,
		nullptr,
		&deviceContext
	);

	if (hr != S_OK) {
		LogError("Could not create DX11 device: ", std::hex, hr);
		return false;
	}

	// Fill the swapchain description structure
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = { 0 };
	swapChainDesc.Stereo = FALSE;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.SampleDesc.Quality = 0;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.Scaling = DXGI_SCALING_NONE;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	swapChainDesc.Flags = 0;
	swapChainDesc.BufferCount = 2;
	swapChainDesc.Width = width;
	swapChainDesc.Height = height;
	swapChainDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;

	// Create the swapchain and associate it to the SDL window
	hr = factory->CreateSwapChainForHwnd(device,
		hwnd,
		&swapChainDesc,
		nullptr,
		nullptr,
		&swapChain);

	if (hr == S_OK) {
		auto dc = GetDC(hwnd);
		gladLoadWGL(dc);
		dxDevice = wglDXOpenDeviceNV(device);
	} else {
		LogError("Could not create DXGI swapchain: ", std::hex, hr);
		return false;
	}

	glGenRenderbuffers(1, &colorRbuf);
	if (depthBuffer)
		glGenRenderbuffers(1, &dsRbuf);
	glGenFramebuffers(1, &fbuf);
	glBindFramebuffer(GL_FRAMEBUFFER, fbuf);

	return true;
}

bool DXGI::OnResize(int width, int height) {
	deviceContext->ClearState();

	Unload();

	if (auto hr = swapChain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0); hr != S_OK) {
		LogError("Could not resize buffers: ", std::hex, hr);
		return false;
	}

	D3D11_VIEWPORT view =
	{
		0.f,
		0.f,
		(float)width,
		(float)height,
		0.f,
		1.f
	};

	deviceContext->RSSetViewports(1, &view);

	return Load();
}

void DXGI::OnDestroy() {
	Unload();

	deviceContext->ClearState();

	glDeleteFramebuffers(1, &fbuf);
	glDeleteRenderbuffers(1, &colorRbuf);
	if (depthBuffer)
		glDeleteRenderbuffers(1, &dsRbuf);

	wglDXCloseDeviceNV(dxDevice);

	deviceContext->ClearState();
	deviceContext->Release();
	device->Release();
	swapChain->Release();
}

inline bool DXGI::Load() {
	if (hr = swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void **>(&colorBuffer)); hr != S_OK) {
		LogError("Could not get colorBuffer: ", std::hex, hr);
		colorBuffer = nullptr;
		return false;
	}

	if (hr = device->CreateRenderTargetView(colorBuffer, NULL, &colorView); hr != S_OK) {
		LogError("Could not create render target view: ", std::hex, hr);
		return false;
	}

	// create depth-stencil view based on size from color buffer
	// this can be cached from previous frame if size has not changed
	// or you could skip depth buffer completely (just use GL only depth renderbuffer)
	if (depthBuffer) {
		D3D11_TEXTURE2D_DESC desc;
		colorBuffer->GetDesc(&desc);
		desc.Format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

		if (hr = device->CreateTexture2D(&desc, NULL, &dsBuffer); hr != S_OK) {
			LogError("Could not create Texture2D: ", std::hex, hr);
			return false;
		}

		if (hr = device->CreateDepthStencilView(dsBuffer, NULL, &dsView); hr != S_OK) {
			LogError("Could not create depth stencil view: ", std::hex, hr);
			return false;
		}
	}

	return true;
}

inline void DXGI::Unload() {
	if (colorBuffer)
		colorBuffer->Release();
	if (colorView)
		colorView->Release();
	if (dsBuffer)
		dsBuffer->Release();
	if (dsView)
		dsView->Release();

	colorBuffer = nullptr;
	colorView = nullptr;
	dsBuffer = nullptr;
	dsView = nullptr;
}

bool DXGI::OnLoop() {
	if (dxObjects[0] = wglDXRegisterObjectNV(dxDevice, colorBuffer, colorRbuf, GL_RENDERBUFFER, WGL_ACCESS_READ_WRITE_NV); !dxObjects[0]) {
		auto error = GetLastError() & 0x00FF;
		switch (error) {
		case ERROR_INVALID_HANDLE:
			LogError("Invalid handle!");
			break;
		case ERROR_INVALID_DATA:
			LogError("Invalid data!");
			break;
		case ERROR_OPEN_FAILED:
			LogError("Open failed!");
			break;
		default:
			LogError("Unknown error!");
			break;
		}

		return false;
	}

	if (depthBuffer) {
		if (dxObjects[1] = wglDXRegisterObjectNV(dxDevice, dsBuffer, dsRbuf, GL_RENDERBUFFER, WGL_ACCESS_READ_WRITE_NV); !dxObjects[1]) {
			LogError("Could not register stencil buffer!");
			return false;
		}
	}

	wglDXLockObjectsNV(dxDevice, depthBuffer ? 2 : 1, dxObjects);

	glBindFramebuffer(GL_FRAMEBUFFER, fbuf);

	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, colorRbuf);
	if (depthBuffer) {
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, dsRbuf);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, dsRbuf);
	}

	return true;
}

void DXGI::SwapBuffers() {
	wglDXUnlockObjectsNV(dxDevice, depthBuffer ? 2 : 1, dxObjects);

	wglDXUnregisterObjectNV(dxDevice, dxObjects[0]);
	if (depthBuffer)
		wglDXUnregisterObjectNV(dxDevice, dxObjects[1]);

	swapChain->Present(1, 0);
}

void DXGI::Bind() {
	glBindFramebuffer(GL_FRAMEBUFFER, fbuf);
}
}
#endif