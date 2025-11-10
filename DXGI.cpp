#ifdef WIN32
#include "DXGI.hpp"

#include <glad/glad_wgl.h>

namespace Fetcko {
DXGI::DXGI(bool depthBuffer) :
	depthBuffer(depthBuffer){

}

DXGI::~DXGI() {
	if (factory)
		factory->Release();
	if (adapter)
		adapter->Release();
	if (output)
		output->Release();
}

std::optional<std::tuple<bool, float, float>> DXGI::GetHdrProperties(int outputIndex) {
	if (lastOutputIndex == outputIndex) return std::nullopt;

	lastOutputIndex = outputIndex;

	if (hr = adapter->EnumOutputs(outputIndex, &output); hr != S_OK) {
		LogError("IDXGIAdapter::EnumOutputs() failed: ", std::hex, hr);
		return std::nullopt;
	}

	//if (factory->IsCurrent()) return std::nullopt;

	IDXGIOutput6 *output6 = nullptr;

	if (hr = output->QueryInterface(__uuidof(IDXGIOutput6), (void **)&output6); hr == S_OK) {
		DXGI_OUTPUT_DESC1 desc;
		output6->GetDesc1(&desc);
		auto maxLuminance = desc.MaxLuminance / 80.0f;
		output6->Release();

		std::vector<DISPLAYCONFIG_PATH_INFO> paths;
		std::vector<DISPLAYCONFIG_MODE_INFO> modes;
		UINT32 flags = QDC_ONLY_ACTIVE_PATHS | QDC_VIRTUAL_MODE_AWARE;
		LONG result = ERROR_SUCCESS;

		do {
			// Determine how many path and mode structures to allocate
			UINT32 pathCount, modeCount;
			result = GetDisplayConfigBufferSizes(flags, &pathCount, &modeCount);

			if (result != ERROR_SUCCESS)
				return std::nullopt;

			// Allocate the path and mode arrays
			paths.resize(pathCount);
			modes.resize(modeCount);

			// Get all active paths and their modes
			result = QueryDisplayConfig(flags, &pathCount, paths.data(), &modeCount, modes.data(), nullptr);

			// The function may have returned fewer paths/modes than estimated
			paths.resize(pathCount);
			modes.resize(modeCount);

			// It's possible that between the call to GetDisplayConfigBufferSizes and QueryDisplayConfig
			// that the display state changed, so loop on the case of ERROR_INSUFFICIENT_BUFFER.
		} while (result == ERROR_INSUFFICIENT_BUFFER);

		if (result != ERROR_SUCCESS)
			return std::nullopt;

		// For each active path
		for (auto &path : paths) {
			// Find the source device name
			DISPLAYCONFIG_SOURCE_DEVICE_NAME sourceName = {};
			sourceName.header.adapterId = path.sourceInfo.adapterId;
			sourceName.header.id = path.sourceInfo.id;
			sourceName.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
			sourceName.header.size = sizeof(sourceName);

			result = DisplayConfigGetDeviceInfo(&sourceName.header);

			if (result != ERROR_SUCCESS)
				return std::nullopt;

			// Found our monitor
			if (!wcsncmp(sourceName.viewGdiDeviceName, desc.DeviceName, std::max(wcslen(sourceName.viewGdiDeviceName), wcslen(desc.DeviceName)))) {
				DISPLAYCONFIG_SDR_WHITE_LEVEL sdrWhiteLevel = {};

				sdrWhiteLevel.header.adapterId = path.targetInfo.adapterId;
				sdrWhiteLevel.header.id = path.targetInfo.id;
				sdrWhiteLevel.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SDR_WHITE_LEVEL;
				sdrWhiteLevel.header.size = sizeof(sdrWhiteLevel);

				result = DisplayConfigGetDeviceInfo(&sdrWhiteLevel.header);

				if (result != ERROR_SUCCESS)
					return std::nullopt;

				return std::tuple<bool, float, float>{
					// Treat all P2020 displays as HDR and all P709 displays as SDR
					desc.ColorSpace >= DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P2020 && desc.ColorSpace <= DXGI_COLOR_SPACE_YCBCR_STUDIO_G24_TOPLEFT_P2020,
					sdrWhiteLevel.SDRWhiteLevel / 1000.0f,
					maxLuminance / (sdrWhiteLevel.SDRWhiteLevel / 1000.0f)
				};
			}
		}
	} else {
		LogError("Could not query output interface: ", std::hex, hr);
		return std::nullopt;
	}

	return std::nullopt;
}

bool DXGI::OnInit(int adapterIndex) {
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

	return true;
}

bool DXGI::OnCreate(HWND hwnd, int width, int height) {
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

	swapChain->GetDesc1(&swapChainDesc);

	glGenRenderbuffers(1, &colorRbuf);
	if (depthBuffer)
		glGenRenderbuffers(1, &dsRbuf);
	glGenFramebuffers(1, &fbuf);
	glBindFramebuffer(GL_FRAMEBUFFER, fbuf);
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

	swapChain->Release();

	deviceContext->ClearState();

	glDeleteFramebuffers(1, &fbuf);
	glDeleteRenderbuffers(1, &colorRbuf);
	if (depthBuffer)
		glDeleteRenderbuffers(1, &dsRbuf);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	wglDXCloseDeviceNV(dxDevice);

	deviceContext->ClearState();
	ID3D11DeviceContext *immediateContext;
	device->GetImmediateContext(&immediateContext);
	immediateContext->ClearState();
	immediateContext->Flush();
	immediateContext->Release();

	if (immediateContext != deviceContext)
		deviceContext->Release();

	device->Release();
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