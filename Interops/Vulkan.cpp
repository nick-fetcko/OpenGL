#include "Vulkan.hpp"

#include <set>
#include <string>

#include "Utils.hpp"

// Find a memory in `memoryTypeBitsRequirement` that includes all of `requiredProperties`
int32_t findProperties(const VkPhysicalDeviceMemoryProperties *pMemoryProperties,
	uint32_t memoryTypeBitsRequirement,
	VkMemoryPropertyFlags requiredProperties) {
	const uint32_t memoryCount = pMemoryProperties->memoryTypeCount;
	for (uint32_t memoryIndex = 0; memoryIndex < memoryCount; ++memoryIndex) {
		const uint32_t memoryTypeBits = (1 << memoryIndex);
		const bool isRequiredMemoryType = memoryTypeBitsRequirement & memoryTypeBits;

		const VkMemoryPropertyFlags properties =
			pMemoryProperties->memoryTypes[memoryIndex].propertyFlags;
		const bool hasRequiredProperties =
			(properties & requiredProperties) == requiredProperties;

		if (isRequiredMemoryType && hasRequiredProperties)
			return static_cast<int32_t>(memoryIndex);
	}

	// failed to find memory type
	return -1;
}

#ifdef WIN32
VkResult GetMemoryWin32HandleKHR(
	VkDevice device,
	const VkMemoryGetWin32HandleInfoKHR *pInfo,
	HANDLE *handle) {
	auto func = (PFN_vkGetMemoryWin32HandleKHR)vkGetDeviceProcAddr(device, "vkGetMemoryWin32HandleKHR");

	if (func)
		return func(device, pInfo, handle);

	return VK_ERROR_EXTENSION_NOT_PRESENT;
}
VkResult GetSemaphoreWin32HandleKHR(
	VkDevice device,
	const VkSemaphoreGetWin32HandleInfoKHR *pGetWin32HandleInfo,
	HANDLE *pHandle) {
	auto func = (PFN_vkGetSemaphoreWin32HandleKHR)vkGetDeviceProcAddr(device, "vkGetSemaphoreWin32HandleKHR");

	if (func)
		return func(device, pGetWin32HandleInfo, pHandle);

	return VK_ERROR_EXTENSION_NOT_PRESENT;
}
#endif

void GetPhysicalDeviceExternalSemaphoreProperties(
	VkInstance instance,
	VkPhysicalDevice physicalDevice,
	const VkPhysicalDeviceExternalSemaphoreInfo *pExternalSemaphoreInfo,
	VkExternalSemaphoreProperties *pExternalSemaphoreProperties) {
	auto func = (PFN_vkGetPhysicalDeviceExternalSemaphorePropertiesKHR)vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceExternalSemaphorePropertiesKHR");

	if (func)
		func(physicalDevice, pExternalSemaphoreInfo, pExternalSemaphoreProperties);
}

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
	void *pUserData
) {
	Fetcko::LoggableClass dummy("ValidationLayer");
	Fetcko::Logger logger;
	logger.SetObject(&dummy);

	switch (messageSeverity) {
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
		logger.LogDebug(pCallbackData->pMessage);
		break;
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
		logger.LogInfo(pCallbackData->pMessage);
		break;
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
		logger.LogWarning(pCallbackData->pMessage);
		break;
	default:
		logger.LogError(pCallbackData->pMessage);
		break;
	}

	return VK_FALSE;
}

VkResult CreateDebugUtilsMessengerEXT(
	VkInstance instance,
	const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
	const VkAllocationCallbacks *pAllocator,
	VkDebugUtilsMessengerEXT *pDebugMessenger) {
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");

	if (func)
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);

	return VK_ERROR_EXTENSION_NOT_PRESENT;
}

void DestroyDebugUtilsMessengerEXT(
	VkInstance instance,
	VkDebugUtilsMessengerEXT debugMessenger,
	const VkAllocationCallbacks *pAllocator) {
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");

	if (func)
		func(instance, debugMessenger, pAllocator);
}

namespace Fetcko {

bool Vulkan::OnInit(const InitArgs &args) {
	this->width = args.width;
	this->height = args.height;
	this->surfaceCallback = [args](VkInstance instance) {
		return reinterpret_cast<VkSurfaceKHR>(args.surfaceCallback(instance));
	};

	InitVulkan();

	return true;
}

bool Vulkan::OnResize(int width, int height) {
	this->width = width;
	this->height = height;

	resized = true;

	return true;
}

void Vulkan::SetFormat(VkFormat format, VkColorSpaceKHR colorSpace, GLenum internalFormat) {
	this->format = format;
	this->colorSpace = colorSpace;
	this->internalFormat = internalFormat;

	formatChanged = true;
}

void Vulkan::OnDestroy() {
	vkDeviceWaitIdle(device);

	glDeleteFramebuffers(1, &fbo);
	glDeleteTextures(1, &color);
	glDeleteMemoryObjectsEXT(1, &mem);
	glDeleteSemaphoresEXT(1, &gl_ready);
	glDeleteSemaphoresEXT(1, &gl_complete);

	vkDestroySemaphore(device, sharedSemaphores.gl_ready, nullptr);
	vkDestroySemaphore(device, sharedSemaphores.gl_complete, nullptr);

	vkDestroyImage(device, sharedTexture.image, nullptr);
	vkFreeMemory(device, sharedTexture.memory, nullptr);
	vkDestroyImageView(device, sharedTexture.view, nullptr);
	vkDestroySampler(device, sharedTexture.sampler, nullptr);

	vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);
	vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
	vkDestroyFence(device, inFlightFence, nullptr);

	vkDestroyCommandPool(device, commandPool, nullptr);

	DestroySwapChain();

	DestroyGraphicsPipeline();

	DestroyRenderPass();

	vkDestroyDevice(device, nullptr);

	if (enableValidationLayers)
		DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);

	vkDestroySurfaceKHR(instance, surface, nullptr);
	vkDestroyInstance(instance, nullptr);
}

bool Vulkan::OnLoop() {
	vkWaitForFences(device, 1, &inFlightFence, VK_TRUE, UINT64_MAX);
	vkResetFences(device, 1, &inFlightFence);

	vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

	vkResetCommandBuffer(commandBuffer, 0);

	RecordCommandBuffer(commandBuffer, imageIndex);

	return true;
}

void Vulkan::InitVulkan() {
	CreateInstance();
	SetupDebugMessenger();
	CreateSurface();
	PickPhysicalDevice();
	CreateLogicalDevice();
	CreateSwapChain();
	CreateImageViews();
	CreateRenderPass();
	CreateGraphicsPipeline();
	CreateFramebuffers();
	CreateCommandPool();
	CreateCommandBuffer();
	CreateSyncObjects();
	CreateSharedResources();
}

bool Vulkan::CheckValidationLayerSupport() {
	uint32_t layerCount = 0;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	for (const char *layerName : ValidationLayers) {
		bool layerFound = false;

		for (const auto &layerProperties : availableLayers) {
			if (strcmp(layerName, layerProperties.layerName) == 0)
				return true;
		}

		if (!layerFound)
			return false;
	}

	return false;
}

std::vector<const char *> Vulkan::GetRequiredExtensions() {
	std::vector<const char *> extensions;

	for (const auto &extension : InstanceExtensions)
		extensions.emplace_back(extension);

	if (enableValidationLayers)
		extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

	return extensions;
}

void Vulkan::PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT &createInfo) {
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	createInfo.messageSeverity =
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	createInfo.messageType =
		VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	createInfo.pfnUserCallback = DebugCallback;
	createInfo.pUserData = nullptr;
}

void Vulkan::CreateInstance() {
	if (enableValidationLayers && !CheckValidationLayerSupport())
		LogError("validation layers requested, but not available!");

	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Hello Triangle";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "No Engine";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_1;

	VkInstanceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	auto requiredExtensions = GetRequiredExtensions();
	createInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size());
	createInfo.ppEnabledExtensionNames = requiredExtensions.data();

	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
	if (enableValidationLayers) {
		createInfo.enabledLayerCount = static_cast<uint32_t>(ValidationLayers.size());
		createInfo.ppEnabledLayerNames = ValidationLayers.data();

		PopulateDebugMessengerCreateInfo(debugCreateInfo);
		createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT *)&debugCreateInfo;
	} else {
		createInfo.enabledLayerCount = 0;
		createInfo.pNext = nullptr;
	}

	uint32_t extensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

	std::vector<VkExtensionProperties> extensions(extensionCount);

	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

	LogDebug("available extensions:");

	for (const auto &extension : extensions)
		LogDebug('\t', extension.extensionName);

	if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS)
		LogError("failed to create instance!");
}

void Vulkan::SetupDebugMessenger() {
	if (!enableValidationLayers) return;

	VkDebugUtilsMessengerCreateInfoEXT createInfo{};
	PopulateDebugMessengerCreateInfo(createInfo);

	if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS)
		LogError("failed to set up debug messanger!");
}

void Vulkan::CreateSurface() {
	surface = surfaceCallback(instance);
}

Vulkan::QueueFamilyIndices Vulkan::FindQueueFamilies(VkPhysicalDevice device) {
	QueueFamilyIndices ret;

	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

	int i = 0;
	for (const auto &queueFamily : queueFamilies) {
		if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
			ret.graphicsFamily = i;

		VkBool32 presentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

		if (presentSupport)
			ret.presentFamily = i;

		if (ret.IsComplete())
			break;

		++i;
	}

	return ret;
}

bool Vulkan::CheckDeviceExtensionSupport(VkPhysicalDevice device) {
	uint32_t extensionCount = 0;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

	std::set<std::string> requiredExtensions(DeviceExtensions.begin(), DeviceExtensions.end());

	for (const auto &extension : availableExtensions)
		requiredExtensions.erase(extension.extensionName);

	return requiredExtensions.empty();
}

Vulkan::SwapChainSupportDetails Vulkan::QuerySwapChainSupport(VkPhysicalDevice device) {
	SwapChainSupportDetails ret;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &ret.capabilities);

	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

	if (formatCount) {
		ret.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, ret.formats.data());
	}

	uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
	if (presentModeCount) {
		ret.presentModes.resize(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, ret.presentModes.data());
	}

	return ret;
}

std::size_t Vulkan::RateDevice(VkPhysicalDevice device) {
	VkPhysicalDeviceProperties deviceProperties;
	vkGetPhysicalDeviceProperties(device, &deviceProperties);

	VkPhysicalDeviceProperties2 deviceProperties2{};
	deviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	vkGetPhysicalDeviceProperties2(device, &deviceProperties2);

	VkPhysicalDeviceFeatures deviceFeatures;
	vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

	auto queueFamilies = FindQueueFamilies(device);

	std::size_t ret = 0;

	if (deviceFeatures.geometryShader &&
		queueFamilies.IsComplete()
		) {

		if (CheckDeviceExtensionSupport(device)) {
			auto swapChainSupport = QuerySwapChainSupport(device);

			if (!swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty()) {
				if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
					ret += 1000;

				ret += deviceProperties.limits.maxImageDimension2D;
			}
		}
	}

	return ret;
}

void Vulkan::PickPhysicalDevice() {
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
	if (!deviceCount)
		LogError("failed to find GPUs with Vulkan support!");

	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

	std::multimap<std::size_t, VkPhysicalDevice> candidates;
	for (const auto &device : devices)
		candidates.insert(std::make_pair(RateDevice(device), device));

	if (candidates.rbegin()->first > 0)
		physicalDevice = candidates.rbegin()->second;
	else
		LogError("failed to find a suitable GPU!");

	if (physicalDevice == VK_NULL_HANDLE)
		LogError("failed to find a suitable GPU!");
}

void Vulkan::CreateLogicalDevice() {
	auto indices = FindQueueFamilies(physicalDevice);

	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	std::set<uint32_t> uniqueQueueFamilies = { *indices.graphicsFamily, *indices.presentFamily };

	float queuePriority = 1.0f;
	for (uint32_t queueFamily : uniqueQueueFamilies) {
		VkDeviceQueueCreateInfo queueCreateInfo{};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamily;
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;
		queueCreateInfos.emplace_back(std::move(queueCreateInfo));
	}

	VkPhysicalDeviceFeatures deviceFeatures{};

	VkDeviceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.pQueueCreateInfos = queueCreateInfos.data();
	createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	createInfo.pEnabledFeatures = &deviceFeatures;
	createInfo.enabledExtensionCount = static_cast<uint32_t>(DeviceExtensions.size());
	createInfo.ppEnabledExtensionNames = DeviceExtensions.data();

	if (enableValidationLayers) {
		createInfo.enabledLayerCount = static_cast<uint32_t>(ValidationLayers.size());
		createInfo.ppEnabledLayerNames = ValidationLayers.data();
	} else {
		createInfo.enabledLayerCount = 0;
	}

	if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS)
		LogError("failed to create logical device!");

	vkGetDeviceQueue(device, *indices.graphicsFamily, 0, &graphicsQueue);
	vkGetDeviceQueue(device, *indices.presentFamily, 0, &presentQueue);
}

VkSurfaceFormatKHR Vulkan::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats) {
	for (const auto &availableFormat : availableFormats) {
		if (availableFormat.format == /* VK_FORMAT_A2B10G10R10_UNORM_PACK32 */ format &&
			availableFormat.colorSpace == /* VK_COLOR_SPACE_HDR10_ST2084_EXT*/ colorSpace)
			return availableFormat;
	}

	LogWarning("Could not find preferred swap surface format! Choosing the first one we found.");

	return availableFormats[4];
}

VkPresentModeKHR Vulkan::ChooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes) {
	for (const auto &availablePresentMode : availablePresentModes) {
		if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
			return availablePresentMode;
	}

	return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D Vulkan::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities) {
	if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
		return capabilities.currentExtent;
	} else {
		VkExtent2D actualExtent = {
			static_cast<uint32_t>(width),
			static_cast<uint32_t>(height)
		};

		actualExtent.width = std::clamp(
			actualExtent.width,
			capabilities.minImageExtent.width,
			capabilities.maxImageExtent.width
		);

		actualExtent.height = std::clamp(
			actualExtent.height,
			capabilities.minImageExtent.height,
			capabilities.maxImageExtent.height
		);

		return actualExtent;
	}
}

void Vulkan::CreateSwapChain() {
	auto swapChainSupport = QuerySwapChainSupport(physicalDevice);

	auto surfaceFormat = ChooseSwapSurfaceFormat(swapChainSupport.formats);
	auto presentMode = ChooseSwapPresentMode(swapChainSupport.presentModes);
	auto extent = ChooseSwapExtent(swapChainSupport.capabilities);

	uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
	if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
		imageCount = swapChainSupport.capabilities.maxImageCount;

	VkSwapchainCreateInfoKHR createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = surface;
	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

	QueueFamilyIndices indices = FindQueueFamilies(physicalDevice);
	uint32_t queueFamilyIndices[] = { *indices.graphicsFamily, *indices.presentFamily };

	if (indices.graphicsFamily != indices.presentFamily) {
		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = queueFamilyIndices;
	} else {
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		createInfo.queueFamilyIndexCount = 0;
		createInfo.pQueueFamilyIndices = nullptr;
	}

	createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;

	createInfo.oldSwapchain = VK_NULL_HANDLE;

	if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS)
		LogError("failed to create swap chain!");

	vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
	swapChainImages.resize(imageCount);
	vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());

	swapChainImageFormat = surfaceFormat.format;
	swapChainExtent = extent;
}

void Vulkan::DestroySwapChain() {
	for (auto framebuffer : swapChainFramebuffers)
		vkDestroyFramebuffer(device, framebuffer, nullptr);

	for (auto imageView : swapChainImageViews)
		vkDestroyImageView(device, imageView, nullptr);

	vkDestroySwapchainKHR(device, swapChain, nullptr);
}

void Vulkan::RecreateSwapChain() {
	vkDeviceWaitIdle(device);

	DestroySwapChain();

	CreateSwapChain();
	if (formatChanged) {
		DestroyRenderPass();
		DestroyGraphicsPipeline();
		CreateRenderPass();
		CreateGraphicsPipeline();
	}
	CreateImageViews();
	CreateFramebuffers();
	CreateSharedResources();
}

void Vulkan::CreateImageViews() {
	swapChainImageViews.resize(swapChainImages.size());

	for (std::size_t i = 0; i < swapChainImages.size(); ++i) {
		VkImageViewCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.image = swapChainImages[i];
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		createInfo.format = swapChainImageFormat;
		createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

		createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		createInfo.subresourceRange.baseMipLevel = 0;
		createInfo.subresourceRange.levelCount = 1;
		createInfo.subresourceRange.baseArrayLayer = 0;
		createInfo.subresourceRange.layerCount = 1;

		if (vkCreateImageView(device, &createInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS)
			LogError("failed to create image view!");
	}
}

void Vulkan::CreateRenderPass() {
	VkAttachmentDescription colorAttachment{};
	colorAttachment.format = swapChainImageFormat;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference colorAttachmentRef{};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;
	VkRenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = 1;
	renderPassInfo.pAttachments = &colorAttachment;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;

	VkSubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;

	if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS)
		LogError("failed to create render pass!");
}

void Vulkan::DestroyRenderPass() {
	vkDestroyRenderPass(device, renderPass, nullptr);
}

VkShaderModule Vulkan::CreateShaderModule(const std::string &code) {
	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = code.size();
	createInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());

	VkShaderModule shaderModule;
	if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
		LogError("failed to create shader module!");

	return shaderModule;
}

void Vulkan::CreateGraphicsPipeline() {
	auto vertShaderCode = Utils::GetStringFromFile(
		Utils::GetResource("vertex.spv")
	);
	auto fragShaderCode = Utils::GetStringFromFile(
		Utils::GetResource("fragment.spv")
	);

	auto vertShaderModule = CreateShaderModule(vertShaderCode);
	auto fragShaderModule = CreateShaderModule(fragShaderCode);

	VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = vertShaderModule;
	vertShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = fragShaderModule;
	fragShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 0;
	vertexInputInfo.pVertexBindingDescriptions = nullptr;
	vertexInputInfo.vertexAttributeDescriptionCount = 0;
	vertexInputInfo.pVertexAttributeDescriptions = nullptr;

	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(swapChainExtent.width);
	viewport.height = static_cast<float>(swapChainExtent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = swapChainExtent;

	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.depthBiasConstantFactor = 0.0f;
	rasterizer.depthBiasClamp = 0.0f;
	rasterizer.depthBiasSlopeFactor = 0.0f;

	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampling.minSampleShading = 1.0f;
	multisampling.pSampleMask = nullptr;
	multisampling.alphaToCoverageEnable = VK_FALSE;
	multisampling.alphaToOneEnable = VK_FALSE;

	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask =
		VK_COLOR_COMPONENT_R_BIT |
		VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT |
		VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;
	colorBlending.blendConstants[0] = 0.0f;
	colorBlending.blendConstants[1] = 0.0f;
	colorBlending.blendConstants[2] = 0.0f;
	colorBlending.blendConstants[3] = 0.0f;

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 0;
	pipelineLayoutInfo.pSetLayouts = nullptr;
	pipelineLayoutInfo.pushConstantRangeCount = 0;
	pipelineLayoutInfo.pPushConstantRanges = nullptr;

	if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
		LogError("failed to create pipeline layout!");

	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStages;
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = nullptr;
	pipelineInfo.pColorBlendState = &colorBlending;
	//pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.layout = pipelineLayout;
	pipelineInfo.renderPass = renderPass;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex = -1;

	if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS)
		LogError("failed to create graphics pipeline!");

	vkDestroyShaderModule(device, fragShaderModule, nullptr);
	vkDestroyShaderModule(device, vertShaderModule, nullptr);
}

void Vulkan::DestroyGraphicsPipeline() {
	vkDestroyPipeline(device, graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
}

void Vulkan::CreateFramebuffers() {
	swapChainFramebuffers.resize(swapChainImageViews.size());

	for (std::size_t i = 0; i < swapChainImageViews.size(); ++i) {
		VkImageView attachments[] = {
			swapChainImageViews[i]
		};

		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = renderPass;
		framebufferInfo.attachmentCount = 1;
		framebufferInfo.pAttachments = attachments;
		framebufferInfo.width = swapChainExtent.width;
		framebufferInfo.height = swapChainExtent.height;
		framebufferInfo.layers = 1;

		if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS)
			LogError("failed to create framebuffer!");
	}
}

void Vulkan::CreateCommandPool() {
	auto queueFamilyIndices = FindQueueFamilies(physicalDevice);

	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	poolInfo.queueFamilyIndex = *queueFamilyIndices.graphicsFamily;

	if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS)
		LogError("failed to create command pool!");
}

void Vulkan::CreateCommandBuffer() {
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = commandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = 1;

	if (vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer) != VK_SUCCESS)
		LogError("failed to allocate command buffers!");
}

void Vulkan::RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = 0;
	beginInfo.pInheritanceInfo = nullptr;

	if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
		LogError("failed to begin recording command buffer!");

	VkRenderPassBeginInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = renderPass;
	renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
	renderPassInfo.renderArea.offset = { 0, 0 };
	renderPassInfo.renderArea.extent = swapChainExtent;

	VkClearValue clearColor = { {{0.0f, 0.0f, 0.0f, 1.0f}} };
	renderPassInfo.clearValueCount = 1;
	renderPassInfo.pClearValues = &clearColor;

	vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

	vkCmdDraw(commandBuffer, 3, 1, 0, 0);

	vkCmdEndRenderPass(commandBuffer);

	VkImageLayout imageLayout{};

	imageCopy.srcOffset = { 0, 0 };
	imageCopy.dstOffset = { 0, 0 };
	imageCopy.extent.depth = 1;
	imageCopy.extent.width = swapChainExtent.width;
	imageCopy.extent.height = swapChainExtent.height;

	imageCopy.srcSubresource = VkImageSubresourceLayers{};
	imageCopy.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageCopy.srcSubresource.layerCount = 1;
	imageCopy.srcSubresource.mipLevel = 0;
	imageCopy.srcSubresource.baseArrayLayer = 0;

	imageCopy.dstSubresource = VkImageSubresourceLayers{};
	imageCopy.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageCopy.dstSubresource.layerCount = 1;
	imageCopy.dstSubresource.mipLevel = 0;
	imageCopy.dstSubresource.baseArrayLayer = 0;

	VkImageMemoryBarrier dstBarrier{};
	dstBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		dstBarrier.pNext = nullptr,
		dstBarrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT; // Or whatever was reading from it
	dstBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	dstBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED; // Or current layout (e.g., VK_IMAGE_LAYOUT_PRESENT_SRC_KHR after acquire)
	dstBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	dstBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	dstBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	dstBarrier.image = swapChainImages[imageIndex];
	dstBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

	vkCmdPipelineBarrier(commandBuffer,
		VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, // Or more specific stage
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		0, 0, nullptr, 0, nullptr, 1, &dstBarrier);

	GLenum srcLayout = GL_LAYOUT_COLOR_ATTACHMENT_EXT;
	glWaitSemaphoreEXT(gl_ready, 0, nullptr, 1, &color, &srcLayout);

	glBindFramebuffer(GL_FRAMEBUFFER, fbo);

	glClearColor(1.0, 0.0, 0.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);
}

bool Vulkan::SwapBuffers() {
	GLenum dstLayout = GL_LAYOUT_SHADER_READ_ONLY_EXT;
	glSignalSemaphoreEXT(gl_complete, 0, nullptr, 1, &color, &dstLayout);

	glFlush();

	vkCmdCopyImage(
		commandBuffer,
		sharedTexture.image,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		swapChainImages[imageIndex],
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1,
		&imageCopy
	);

	VkImageMemoryBarrier presentBarrier{};
	presentBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		presentBarrier.pNext = nullptr;
	presentBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	presentBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT; // For presentation
	presentBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	presentBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	presentBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	presentBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	presentBarrier.image = swapChainImages[imageIndex];
	presentBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	vkCmdPipelineBarrier(commandBuffer,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, // Or more specific stage
		0, 0, nullptr, 0, nullptr, 1, &presentBarrier);

	if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
		LogError("failed to record command buffer!");

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkSemaphore waitSemaphores[] = { imageAvailableSemaphore, sharedSemaphores.gl_complete };
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT };
	submitInfo.waitSemaphoreCount = 2;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	VkSemaphore signalSemaphores[] = { renderFinishedSemaphore, sharedSemaphores.gl_ready };
	submitInfo.signalSemaphoreCount = 2;
	submitInfo.pSignalSemaphores = signalSemaphores;

	if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFence) != VK_SUCCESS)
		LogError("failed to submit draw command buffer!");

	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;

	VkSwapchainKHR swapChains[] = { swapChain };
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &imageIndex;
	presentInfo.pResults = nullptr;

	if (vkQueuePresentKHR(presentQueue, &presentInfo) != VK_SUCCESS || resized || formatChanged) {
		RecreateSwapChain();
		resized = false;
		formatChanged = false;

		return false;
	}

	return true;
}

void Vulkan::CreateSyncObjects() {
	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphore) != VK_SUCCESS ||
		vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphore) != VK_SUCCESS ||
		vkCreateFence(device, &fenceInfo, nullptr, &inFlightFence) != VK_SUCCESS)
		LogError("failed to create semaphors and fence!");
}

// Derived from https://github.com/KhronosGroup/Vulkan-Samples/blob/main/samples/extensions/open_gl_interop/open_gl_interop.cpp
// and https://docs.vulkan.org/refpages/latest/refpages/source/VkPhysicalDeviceMemoryProperties.html
void Vulkan::CreateSharedResources() {
	vkDestroyImage(device, sharedTexture.image, nullptr);
	vkFreeMemory(device, sharedTexture.memory, nullptr);
	vkDestroyImageView(device, sharedTexture.view, nullptr);
	vkDestroySampler(device, sharedTexture.sampler, nullptr);

	glDeleteTextures(1, &color);
	glDeleteMemoryObjectsEXT(1, &mem);
	glDeleteFramebuffers(1, &fbo);
	glDeleteSemaphoresEXT(1, &gl_ready);
	glDeleteSemaphoresEXT(1, &gl_complete);

	vkDestroySemaphore(device, sharedSemaphores.gl_ready, nullptr);
	vkDestroySemaphore(device, sharedSemaphores.gl_complete, nullptr);

	VkExternalSemaphoreHandleTypeFlagBits flags[] = {
			VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT,
			VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT,
			VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT,
			VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT,
			VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT };

	VkPhysicalDeviceExternalSemaphoreInfo zzzz {
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_SEMAPHORE_INFO, nullptr };
	VkExternalSemaphoreProperties aaaa{ VK_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_PROPERTIES,
									   nullptr };

	bool                                  found = false;
	VkExternalSemaphoreHandleTypeFlagBits compatable_semaphore_type;
	for (size_t i = 0; i < 5; i++) {
		zzzz.handleType = flags[i];
		GetPhysicalDeviceExternalSemaphoreProperties(instance, physicalDevice, &zzzz, &aaaa);
		if (aaaa.compatibleHandleTypes & flags[i] && aaaa.externalSemaphoreFeatures & VK_EXTERNAL_SEMAPHORE_FEATURE_EXPORTABLE_BIT)
		{
			compatable_semaphore_type = flags[i];
			found = true;
			break;
		}
	}

	if (!found) {
		throw;
	}

	VkExportSemaphoreCreateInfo exportSemaphoreCreateInfo{
		VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO, nullptr,
		static_cast<VkExternalSemaphoreHandleTypeFlags>(compatable_semaphore_type) };
	VkSemaphoreCreateInfo semaphoreCreateInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
											  &exportSemaphoreCreateInfo };
	if (vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr,
		&sharedSemaphores.gl_complete) != VK_SUCCESS)
		LogError("could not create semaphore!");
	if (vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr,
		&sharedSemaphores.gl_ready) != VK_SUCCESS)
		LogError("could not create semaphore!");

#if WIN32
	VkSemaphoreGetWin32HandleInfoKHR semaphoreGetHandleInfo{
		VK_STRUCTURE_TYPE_SEMAPHORE_GET_WIN32_HANDLE_INFO_KHR, nullptr,
		VK_NULL_HANDLE, compatable_semaphore_type };
	semaphoreGetHandleInfo.semaphore = sharedSemaphores.gl_ready;
	if (GetSemaphoreWin32HandleKHR(device, &semaphoreGetHandleInfo, &shareHandles.gl_ready) != VK_SUCCESS)
		LogError("could not get Win32 handle for semaphore!");
	semaphoreGetHandleInfo.semaphore = sharedSemaphores.gl_complete;
	if (GetSemaphoreWin32HandleKHR(device, &semaphoreGetHandleInfo, &shareHandles.gl_complete) != VK_SUCCESS)
		LogError("could not get Win32 handle for semaphore!");
#else
	VkSemaphoreGetFdInfoKHR semaphoreGetFdInfo{
		VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR, nullptr,
		VK_NULL_HANDLE, compatable_semaphore_type };
	semaphoreGetFdInfo.semaphore = sharedSemaphores.gl_ready;
	VK_CHECK(vkGetSemaphoreFdKHR(deviceHandle, &semaphoreGetFdInfo, &shareHandles.gl_ready));
	semaphoreGetFdInfo.semaphore = sharedSemaphores.gl_complete;
	VK_CHECK(vkGetSemaphoreFdKHR(deviceHandle, &semaphoreGetFdInfo, &shareHandles.gl_complete));
#endif

	VkExternalMemoryImageCreateInfo external_memory_image_create_info{ VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO };
#if WIN32
	external_memory_image_create_info.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT_KHR;
#else
	external_memory_image_create_info.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHR;
#endif
	VkImageCreateInfo imageCreateInfo{};
	imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCreateInfo.pNext = &external_memory_image_create_info;
	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	imageCreateInfo.format = swapChainImageFormat;
	imageCreateInfo.mipLevels = 1;
	imageCreateInfo.arrayLayers = 1;
	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCreateInfo.extent.depth = 1;
	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageCreateInfo.extent.width = swapChainExtent.width;
	imageCreateInfo.extent.height = swapChainExtent.height;
	imageCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	if (vkCreateImage(device, &imageCreateInfo, nullptr, &sharedTexture.image) != VK_SUCCESS)
		LogError("could not allocate shared texture!");

	VkMemoryDedicatedAllocateInfo dedicated_allocate_info;
	dedicated_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
	dedicated_allocate_info.pNext = nullptr;
	dedicated_allocate_info.buffer = VK_NULL_HANDLE;
	dedicated_allocate_info.image = sharedTexture.image;

	VkMemoryRequirements memReqs{};
	vkGetImageMemoryRequirements(device, sharedTexture.image, &memReqs);

	// In order to export an external handle later, we need to tell it explicitly during memory allocation
	VkExportMemoryAllocateInfo export_memory_allocate_Info;
	export_memory_allocate_Info.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
	export_memory_allocate_Info.pNext = &dedicated_allocate_info;
#if WIN32
	export_memory_allocate_Info.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT_KHR;
#else
	export_memory_allocate_Info.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHR;
#endif

	VkMemoryAllocateInfo memAllocInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, &export_memory_allocate_Info };

	VkMemoryRequirements memoryRequirements;
	vkGetImageMemoryRequirements(device, sharedTexture.image, &memoryRequirements);
	VkPhysicalDeviceMemoryProperties memoryProperties{};

	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

	int32_t memoryType =
		findProperties(&memoryProperties, memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	if (memoryType == -1) // not found; try fallback properties
		memoryType =
		findProperties(&memoryProperties, memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	memAllocInfo.allocationSize = sharedTexture.allocationSize = memReqs.size;
	memAllocInfo.memoryTypeIndex = memoryType;
	if (vkAllocateMemory(device, &memAllocInfo, nullptr, &sharedTexture.memory) != VK_SUCCESS)
		LogError("could not allocate shared texture memory!");

	if (vkBindImageMemory(device, sharedTexture.image, sharedTexture.memory, 0) != VK_SUCCESS)
		LogError("could not allocate shared texture memory!");

#if WIN32
	VkMemoryGetWin32HandleInfoKHR memoryFdInfo{ VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR, nullptr,
											   sharedTexture.memory,
											   VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT };
	if (GetMemoryWin32HandleKHR(device, &memoryFdInfo, &shareHandles.memory) != VK_SUCCESS)
		LogError("could not get win32 handle!");
#else
	VkMemoryGetFdInfoKHR memoryFdInfo{ VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR, nullptr,
									  sharedTexture.memory,
									  VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT };
	VK_CHECK(vkGetMemoryFdKHR(deviceHandle, &memoryFdInfo, &shareHandles.memory));
#endif

	// Calculate valid filter and mipmap modes
	VkFilter            filter = VK_FILTER_LINEAR;
	VkSamplerMipmapMode mipmap_mode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	//vkb::make_filters_valid(get_device().get_gpu().get_handle(), imageCreateInfo.format, &filter, &mipmap_mode);

	// Create sampler
	VkSamplerCreateInfo samplerCreateInfo{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
	samplerCreateInfo.magFilter = filter;
	samplerCreateInfo.minFilter = filter;
	samplerCreateInfo.mipmapMode = mipmap_mode;
	samplerCreateInfo.maxLod = static_cast<float>(1);
	// samplerCreateInfo.maxAnisotropy = context.deviceFeatures.samplerAnisotropy ? context.deviceProperties.limits.maxSamplerAnisotropy : 1.0f;
	// samplerCreateInfo.anisotropyEnable = context.deviceFeatures.samplerAnisotropy;
	samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	vkCreateSampler(device, &samplerCreateInfo, nullptr, &sharedTexture.sampler);

	// Create image view
	VkImageViewCreateInfo viewCreateInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewCreateInfo.image = sharedTexture.image;
	viewCreateInfo.format = swapChainImageFormat;
	viewCreateInfo.subresourceRange = VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1,
															  0, 1 };
	vkCreateImageView(device, &viewCreateInfo, nullptr, &sharedTexture.view);

	gladLoadGL();

	// Create the texture for the FBO color attachment.
	// This only reserves the ID, it doesn't allocate memory
	glGenTextures(1, &color);
	glBindTexture(GL_TEXTURE_2D, color);

	// Create the GL identifiers

	// semaphores
	glGenSemaphoresEXT(1, &gl_ready);
	glGenSemaphoresEXT(1, &gl_complete);
	// memory
	glCreateMemoryObjectsEXT(1, &mem);

	GLint dedicated = GL_TRUE;
	glMemoryObjectParameterivEXT(mem, GL_DEDICATED_MEMORY_OBJECT_EXT, &dedicated);

	// Platform specific import.
	glImportSemaphoreWin32HandleEXT(gl_ready, GL_HANDLE_TYPE_OPAQUE_WIN32_EXT, shareHandles.gl_ready);
	glImportSemaphoreWin32HandleEXT(gl_complete, GL_HANDLE_TYPE_OPAQUE_WIN32_EXT, shareHandles.gl_complete);
	glImportMemoryWin32HandleEXT(mem, sharedTexture.allocationSize, GL_HANDLE_TYPE_OPAQUE_WIN32_EXT, shareHandles.memory);

	// Use the imported memory as backing for the OpenGL texture.  The internalFormat, dimensions
	// and mip count should match the ones used by Vulkan to create the image and determine it's memory
	// allocation.
	glTextureStorageMem2DEXT(color, 1, internalFormat, swapChainExtent.width,
		swapChainExtent.height, mem, 0);
	glBindTexture(GL_TEXTURE_2D, 0);

	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, color, 0);
}

}