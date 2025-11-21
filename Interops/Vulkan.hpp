#pragma once

#include <optional>
#include <vector>

#include <glad/glad.h>

#include <vulkan/vulkan.h>

#if defined(WIN32)
#include <windows.h>
#include <vulkan/vulkan_win32.h>

using Handle = HANDLE;
#else
#include <vulkan/vulkan_wayland.h>
using Handle = int;
constexpr Handle INVALID_HANDLE_VALUE = static_cast<Handle>(-1);
#endif

#include "Interop.hpp"

#include "Logger.hpp"

namespace Fetcko {
// Derived from https://vulkan-tutorial.com/Drawing_a_triangle/Setup/Base_code
class Vulkan : public Interop, public LoggableClass {
private:
	const inline static std::vector<const char *> ValidationLayers = {
		"VK_LAYER_KHRONOS_validation"
	};

	const inline static std::vector<const char *> InstanceExtensions = {
		VK_KHR_SURFACE_EXTENSION_NAME,
		VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME,
		VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
		VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME,
#ifdef __linux__
		VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME,
		// Can't include vulkan_xlib.h because
		// it conflicts with vulkan_wayland.h
		"VK_KHR_xlib_surface"
#endif
#ifdef WIN32
		VK_KHR_WIN32_SURFACE_EXTENSION_NAME
#endif
	};

	const inline static std::vector<const char *> DeviceExtensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
		VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME,
#ifdef WIN32
		VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME,
		VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME
#else
		VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
		VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME
#endif
	};

	constexpr static bool enableValidationLayers =
#if !defined(NDEBUG) && !defined(USING_FLATPAK)
		true
#else
		false
#endif
		;

	struct QueueFamilyIndices {
		std::optional<uint32_t> graphicsFamily = std::nullopt;
		std::optional<uint32_t> presentFamily = std::nullopt;

		bool IsComplete() {
			return
				graphicsFamily.has_value() &&
				presentFamily.has_value();
		}
	};

	struct SwapChainSupportDetails {
		VkSurfaceCapabilitiesKHR capabilities;
		std::vector<VkSurfaceFormatKHR> formats;
		std::vector<VkPresentModeKHR> presentModes;
	};

public:
	bool OnInit(const InitArgs &args) override;

	bool OnResize(int width, int height) override;

	void OnDestroy() override;

	bool OnLoop() override;

	bool SwapBuffers() override;

	void SetFormat(VkFormat format, VkColorSpaceKHR colorSpace, GLenum internalFormat);

	const VkFormat GetSwapchainImageFormat() const { return swapChainImageFormat; }

private:
	void InitVulkan();

	bool CheckValidationLayerSupport();
	std::vector<const char *> GetRequiredExtensions();
	void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT &createInfo);
	QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);
	bool CheckDeviceExtensionSupport(VkPhysicalDevice device);
	SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device);
	std::pair<std::string, std::size_t> RateDevice(VkPhysicalDevice device);

	VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats);
	VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes);
	VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities);

	VkShaderModule CreateShaderModule(const std::string &code);

	void RecordCommandBuffer(VkCommandBuffer commandBuffer);

	void CreateInstance();
	void SetupDebugMessenger();
	void CreateSurface();
	void PickPhysicalDevice();
	void CreateLogicalDevice();
	void CreateSwapChain();
	void DestroySwapChain();
	void RecreateSwapChain();
	void CreateImageViews();
	void CreateRenderPass();
	void DestroyRenderPass();
	void CreateGraphicsPipeline();
	void DestroyGraphicsPipeline();
	void CreateFramebuffers();
	void CreateCommandPool();
	void CreateCommandBuffer();
	void CreateSyncObjects();
	void DestroySyncObjects();
	void CreateSharedResources();

	VkInstance instance = VK_NULL_HANDLE;
	VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	VkDevice device = VK_NULL_HANDLE;

	VkQueue graphicsQueue = VK_NULL_HANDLE;
	VkQueue presentQueue = VK_NULL_HANDLE;

	VkSurfaceKHR surface = VK_NULL_HANDLE;

	VkSwapchainKHR swapChain = VK_NULL_HANDLE;
	VkFormat swapChainImageFormat;
	VkExtent2D swapChainExtent;

	std::vector<VkImage> swapChainImages;
	std::vector<VkImageView> swapChainImageViews;

	VkRenderPass renderPass = VK_NULL_HANDLE;
	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;

	VkPipeline graphicsPipeline = VK_NULL_HANDLE;

	std::vector<VkFramebuffer> swapChainFramebuffers;

	VkCommandPool commandPool = VK_NULL_HANDLE;
	VkCommandBuffer commandBuffer = VK_NULL_HANDLE;

	std::vector<VkSemaphore> imageAvailableSemaphores;
	std::vector<VkSemaphore> renderFinishedSemaphores;
	VkFence inFlightFence;

	struct SharedTexture {
		VkImage        image{ VK_NULL_HANDLE };
		VkDeviceMemory memory{ VK_NULL_HANDLE };
		VkDeviceSize   size{ 0 };
		VkDeviceSize   allocationSize{ 0 };
		VkSampler      sampler{ VK_NULL_HANDLE };
		VkImageView    view{ VK_NULL_HANDLE };
	};

	SharedTexture sharedTexture;

	struct ShareHandles {
		Handle memory{ INVALID_HANDLE_VALUE };
		Handle gl_ready{ INVALID_HANDLE_VALUE };
		Handle gl_complete{ INVALID_HANDLE_VALUE };
	};

	ShareHandles shareHandles;

	GLuint color = 0;
	GLuint mem = 0;
	GLuint gl_ready{ 0 }, gl_complete{ 0 };

	struct Semaphores {
		VkSemaphore gl_ready{ VK_NULL_HANDLE };
		VkSemaphore gl_complete{ VK_NULL_HANDLE };
	};

	Semaphores sharedSemaphores;

	int width = 0;
	int height = 0;
	std::function<VkSurfaceKHR(VkInstance)> surfaceCallback;

	VkImageCopy imageCopy{};
	uint32_t imageIndex = 0;
	uint32_t lastImageIndex = 0;

	VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
	VkColorSpaceKHR colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
	GLenum internalFormat = GL_RGBA8;

	bool resized = false;
	bool formatChanged = false;

	bool swapChainRecreated = false;
};
}