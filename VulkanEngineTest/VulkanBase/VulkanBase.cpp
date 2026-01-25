// 模拟预处理头
#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#define NOMINMAX
#endif
#include <vulkan/vulkan.h>

#include "VulkanBase.h"

#include <iostream>
#include <format>
#include <vector>
#include <set>
#include <limits>
#include <algorithm>

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData)
{

	switch (messageSeverity)
	{
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
		std::cout << "VERBOSE: " << pCallbackData->pMessage << std::endl;
		break;
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
		std::cout << "INFO: " << pCallbackData->pMessage << std::endl;
		break;
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
		std::cout << "WARNING: " << pCallbackData->pMessage << std::endl;
		break;
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
		std::cout << "ERROR: " << pCallbackData->pMessage << std::endl;
		break;
	default:
		break;
	}

	return VK_FALSE;
}

static void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
{
	createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	createInfo.pfnUserCallback = debugCallback;
}

static void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator)
{
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != nullptr)
	{
		func(instance, debugMessenger, pAllocator);
	}
}

////////////////////////// VulkanBase ////////////////////////////////////////////

VulkanBase::VulkanBase()
	: _api_version(VK_API_VERSION_1_0)
	, _instance(VK_NULL_HANDLE)
	, _device(VK_NULL_HANDLE)
	, _debug_messenger(VK_NULL_HANDLE)
	, _physical_device(VK_NULL_HANDLE)
	, _graphics_queue(VK_NULL_HANDLE)
	, _surface(VK_NULL_HANDLE)
	, _swap_chain(VK_NULL_HANDLE)
	, _present_queue(VK_NULL_HANDLE)
{}

VulkanBase::~VulkanBase()
{}

VulkanBase& VulkanBase::Base()
{
	static VulkanBase base;
	return base;
}

bool VulkanBase::InitVulkanInstance()
{
	_use_lastest_api_version();
	_create_instance();
#ifndef NDEBUG
	_setup_debug_messenger();
#else
#ifdef UseDebugMessenger
	_setup_debug_messenger_in_release();
#endif	// UseDebugMessenger
#endif // !NDEBUG
	return true;
}

bool VulkanBase::InitVulkan()
{
	CreateSurface();
	_pick_physical_device();
	_create_logical_device();
	_create_swap_chain();
	return true;
}

void VulkanBase::CleanUp() const
{
#if defined(UseDebugMessenger) || !defined(NDEBUG)
	DestroyDebugUtilsMessengerEXT(_instance, _debug_messenger, nullptr);
#endif

	vkDestroySwapchainKHR(_device, _swap_chain, nullptr);
	vkDestroyDevice(_device, nullptr);
	vkDestroySurfaceKHR(_instance, _surface, nullptr);
	vkDestroyInstance(_instance, nullptr);

}

// layers
static std::vector<const char*> validationLayers;
static std::vector<const char*> instanceExtensions;
static std::vector<const char*> deviceExtensions;

void VulkanBase::AddValidationLayer(const char* layerName)
{
	validationLayers.push_back(layerName);
}
void VulkanBase::AddInstanceExtension(const char* extensionName)
{
	instanceExtensions.push_back(extensionName);
}
void VulkanBase::AddDeviceExtension(const char* extensionName)
{
	deviceExtensions.push_back(extensionName);
}

bool VulkanBase::_create_instance()
{
#if defined(UseDebugMessenger) || !defined(NDEBUG)
	AddValidationLayer("VK_LAYER_KHRONOS_validation");
	AddInstanceExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Vulkan Test";
	appInfo.apiVersion = _api_version;
	
	////////////////////////       VkInstanceCreateInfo ///////////
	VkInstanceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

#if defined(UseDebugMessenger) || !defined(NDEBUG)
	if (auto check = !_check_validation_layers())
	{
		return false;
	}
	createInfo.enabledLayerCount = uint32_t(validationLayers.size());
	createInfo.ppEnabledLayerNames = validationLayers.data();
	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
	populateDebugMessengerCreateInfo(debugCreateInfo);
	createInfo.pNext = (void*)&debugCreateInfo;
#else
	createInfo.enabledLayerCount = 0;
	createInfo.pNext = nullptr;
#endif
	createInfo.enabledExtensionCount = uint32_t(instanceExtensions.size());
	createInfo.ppEnabledExtensionNames = instanceExtensions.data();

	if (vkCreateInstance(&createInfo, nullptr, &_instance) != VK_SUCCESS)
	{
		std::cout << "ERROR : failed to create instance!\n";
		return false;
	}

	std::cout << std::format("INFO : Create Instance done, instance Extensions Num : {}  :\n", instanceExtensions.size());
	for (auto& name : instanceExtensions)
	{
		std::cout << "\t" << name << ";\n";
	}

	return true;
}

void VulkanBase::_use_lastest_api_version()
{
	if (vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkEnumerateInstanceVersion"))
	{
		if (VkResult result = vkEnumerateInstanceVersion(&_api_version))
		{
			std::cout << std::format("vkEnumerateInstanceVersion error : {}  \n", (int)result);
		}
		return;
	}
	std::cout << std::format("not found vkEnumerateInstanceVersion  \n");
}

bool VulkanBase::_setup_debug_messenger()
{
	VkDebugUtilsMessengerCreateInfoEXT debugMsgInfo{};
	debugMsgInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	debugMsgInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | 
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | 
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	debugMsgInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | 
		VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | 
		VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	debugMsgInfo.pfnUserCallback = debugCallback;
	debugMsgInfo.pUserData = nullptr; // 自定义指针，直接传递到回调函数

	PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessenger =
		reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(_instance, "vkCreateDebugUtilsMessengerEXT"));
	if (vkCreateDebugUtilsMessenger)
	{
		if (vkCreateDebugUtilsMessenger(_instance, &debugMsgInfo, nullptr, &_debug_messenger) != VK_SUCCESS)
		{
			std::cout << std::format("ERRORR : failed to set up debug messenger! \n");
			return false;
		}
	}
	else
	{
		std::cout << std::format("ERRORR : not found vkCreateDebugUtilsMessengerEXT \n");
		return false;
	}
	return true;
}

#ifdef UseDebugMessenger
bool VulkanBase::_setup_debug_messenger_in_release()
{
	return _setup_debug_messenger();
}
#endif	// UseDebugMessenger

bool VulkanBase::_check_validation_layers()
{
	uint32_t layerCount;
	if (VkResult result = vkEnumerateInstanceLayerProperties(&layerCount, nullptr))
	{
		std::cout << std::format(" ERROR : [ VulkanBase ] Failed to get the count of instance layers! Error code: {}\n", int32_t(result));
		return false;
	}
	if (layerCount)
	{
		std::vector<VkLayerProperties> availableLayers(layerCount);
		if (VkResult result = vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data()))
		{
			std::cout << std::format("ERROR : [ VulkanBase ] Failed to enumerate instance layer properties! Error code: {}\n", int32_t(result));
			return false;
		}
		bool found = false;
		for (auto& i : validationLayers)
		{
			for (auto& j : availableLayers)
				if (!strcmp(i, j.layerName))
				{
					found = true;
					break;
				}
			if (!found)
				i = nullptr;
		}
	}
	else
	{
		validationLayers.clear();
	}
	std::cout << std::format("INFO : Check instanceLayers done, Num : {}  :\n", validationLayers.size());
	for (auto& name : validationLayers)
	{
		std::cout << "\t" << name << ";\n";
	}
	return true;
}

bool VulkanBase::_check_device_extension_support(VkPhysicalDevice device)
{
	uint32_t extensionCount;
	if (VkResult result = vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr))
	{
		std::cout << std::format(" ERROR : [ VulkanBase ] Failed to get the count of device extension! Error code: {}\n", int32_t(result));
		return false;
	}
	if (extensionCount)
	{
		std::vector<VkExtensionProperties> availableExtensions(extensionCount);
		if (VkResult result = vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data()))
		{
			std::cout << std::format("ERROR : [ VulkanBase ] Failed to enumerate device extension properties! Error code: {}\n", int32_t(result));
			return false;
		}
		bool found = false;
		for (auto& i : deviceExtensions)
		{
			for (auto& j : availableExtensions)
				if (!strcmp(i, j.extensionName))
				{
					found = true;
					break;
				}
			if (!found)
				i = nullptr;
		}
	}
	else
	{
		deviceExtensions.clear();
	}
	std::cout << std::format("INFO : Check deviceExtensions done, Num : {}  :\n", deviceExtensions.size());
	for (auto& name : deviceExtensions)
	{
		std::cout << "\t" << name << ";\n";
	}
	return true;
}

bool VulkanBase::_pick_physical_device()
{
	uint32_t deviceCount = 0;
	if (VkResult result = vkEnumeratePhysicalDevices(_instance, &deviceCount, nullptr))
	{
		std::cout << std::format(" ERROR : [ VulkanBase ] Failed to get the count of physical devices! Error code: {}\n", int32_t(result));
		return false;
	}
	if (deviceCount == 0)
	{
		std::cout << std::format(" ERROR : [ VulkanBase ] Failed to find GPUs with Vulkan support!\n");
		return false;
	}
	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(_instance, &deviceCount, devices.data());

	// 选择显卡
	auto isDeviceSuitable = [this](VkPhysicalDevice device)->bool {
		VkPhysicalDeviceProperties deviceProperties;
		VkPhysicalDeviceFeatures deviceFeatures;
		vkGetPhysicalDeviceProperties(device, &deviceProperties);
		vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

		_queue_family_indices = _find_queue_families(device);

		auto swapChainAdequate = [this, device]()->bool {
			_swap_chain_support = _query_swap_chain_support(device);
			return !_swap_chain_support.Formats.empty() && !_swap_chain_support.PresentModes.empty();
			};

		return deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU 
			&& deviceFeatures.geometryShader 
			&& _queue_family_indices.IsComplete()
			&& _check_device_extension_support(device)
			&& swapChainAdequate();
		};

	for (const auto& device : devices)
	{
		if (isDeviceSuitable(device))
		{
			_physical_device = device;
			break;
		}
	}

	if (_physical_device == VK_NULL_HANDLE)
	{
		std::cout << "failed to find a suitable GPU!";
		return false;
	}

	return true;
}

bool VulkanBase::_create_logical_device()
{
	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	std::set<uint32_t> uniqueQueueFamilies = { _queue_family_indices.GraphicsFamily,_queue_family_indices.PresentFamily };
	float queuePriority = 1.0f;
	for (auto queueFamily : uniqueQueueFamilies)
	{
		auto& queueCreateInfo = queueCreateInfos.emplace_back();
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamily;
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;
	}

	VkPhysicalDeviceFeatures deviceFeatures{};

	VkDeviceCreateInfo deviceCreateInfo{};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
	deviceCreateInfo.queueCreateInfoCount = (uint32_t)queueCreateInfos.size();
	deviceCreateInfo.pEnabledFeatures = &deviceFeatures;
	deviceCreateInfo.enabledExtensionCount = (uint32_t)deviceExtensions.size();
	deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
#if defined(UseDebugMessenger) || !defined(NDEBUG)
	deviceCreateInfo.enabledLayerCount = (uint32_t)validationLayers.size();
	deviceCreateInfo.ppEnabledLayerNames = validationLayers.data();
#else
	deviceCreateInfo.enabledLayerCount = 0;
#endif
	

	if (VkResult result = vkCreateDevice(_physical_device, &deviceCreateInfo, nullptr, &_device))
	{
		std::cout << std::format(" ERROR : [ VulkanBase ] failed to create logical device! Error code: {}\n", int32_t(result));
		return false;
	}

	vkGetDeviceQueue(_device, _queue_family_indices.GraphicsFamily, 0, &_graphics_queue);
	vkGetDeviceQueue(_device, _queue_family_indices.PresentFamily, 0, &_present_queue);

	return true;
}

bool VulkanBase::CreateSurface()
{
	return true;
}

bool VulkanBase::_create_swap_chain()
{
	auto surfaceFormat = _choose_swap_surface_format(_swap_chain_support.Formats);
	auto presentMode = _choose_swap_presenta_mode(_swap_chain_support.PresentModes);
	auto extent = _choose_swap_extent(_swap_chain_support.Capabilities);

	uint32_t imageCount = _swap_chain_support.Capabilities.minImageCount + 1;
	if (_swap_chain_support.Capabilities.maxImageCount > 0 && imageCount > _swap_chain_support.Capabilities.maxImageCount)
	{
		imageCount = _swap_chain_support.Capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = _surface;
	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1;
	/*
		VK_IMAGE_USAGE_TRANSFER_SRC_BIT					可作为传输操作的源			截图、图像复制
		VK_IMAGE_USAGE_TRANSFER_DST_BIT					可作为传输操作的目标		纹理上传、图像填充
		VK_IMAGE_USAGE_SAMPLED_BIT						可被着色器采样				纹理、输入附件
		VK_IMAGE_USAGE_STORAGE_BIT						可作为存储图像（UAV）		计算着色器输出
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT		可用作深度 / 模板附件		深度测试、阴影
		VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT				可用作输入附件				延迟渲染、子通道输入
		VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT			临时附件（懒内存）			MSAA 解析附件
		VK_IMAGE_USAGE_FRAGMENT_DENSITY_MAP_BIT_EXT		片段密度图					VRS（可变速率着色）
	*/
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	uint32_t queueFamilyIndices[] = { _queue_family_indices.GraphicsFamily, _queue_family_indices.PresentFamily };
	if (_queue_family_indices.GraphicsFamily != _queue_family_indices.PresentFamily)
	{
		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = queueFamilyIndices;
	}
	else
	{
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		createInfo.queueFamilyIndexCount = 0;
		createInfo.pQueueFamilyIndices = nullptr;
	}
	createInfo.preTransform = _swap_chain_support.Capabilities.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;
	//
	createInfo.oldSwapchain = VK_NULL_HANDLE;

	// 
	if (VkResult result = vkCreateSwapchainKHR(_device, &createInfo, nullptr, &_swap_chain))
	{
		std::cout << std::format("ERROR : [ VulkanBase ] Failed to create a swapchain! Error code: {}\n", int32_t(result));
		return false;
	}

	uint32_t swapchainImageCount;
	if (VkResult result = vkGetSwapchainImagesKHR(_device, _swap_chain, &swapchainImageCount, nullptr))
	{
		std::cout << std::format("ERROR : [ VulkanBase ] Failed to get the count of swapchain images! Error code: {}\n", int32_t(result));
		return false;
	}
	_swap_chain_images.resize(swapchainImageCount);
	if (VkResult result = vkGetSwapchainImagesKHR(_device, _swap_chain, &swapchainImageCount, _swap_chain_images.data()))
	{
		std::cout << std::format("ERROR : [ VulkanBase ] Failed to get swapchain images! Error code: {}\n", int32_t(result));
		return false;
	}

	return true;
}

VkSurfaceFormatKHR VulkanBase::_choose_swap_surface_format(const std::vector<VkSurfaceFormatKHR>& available_formats)
{
	for (const auto& availableFormat : available_formats)
	{
		if ((availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB) && (availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR))
		{
			return availableFormat;
		}
	}
	return available_formats[0];
}

VkPresentModeKHR VulkanBase::_choose_swap_presenta_mode(const std::vector<VkPresentModeKHR>& available_present_modes, VkPresentModeKHR mode)
{
	for (const auto& availablePresentMode : available_present_modes)
	{
		if (availablePresentMode == mode)
		{
			return availablePresentMode;
		}
	}
	return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanBase::_choose_swap_extent(const VkSurfaceCapabilitiesKHR& capabilities)
{
	if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
	{
		return capabilities.currentExtent;
	}
	else
	{
		VkExtent2D actualExtent = { _frame_buffer_width, _frame_buffer_height };
		actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
		actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
		return actualExtent;
	}
	
}

VulkanBase::QueueFamilyIndices VulkanBase::_find_queue_families(VkPhysicalDevice device)
{
	QueueFamilyIndices indices;

	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

	uint32_t i = 0;
	for (const auto& queueFamily : queueFamilies)
	{
		if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			indices.GraphicsFamily = i;
			indices.HasGraphicsFamily = true;
		}

		VkBool32 presentSupport = false;
		if (VkResult result = vkGetPhysicalDeviceSurfaceSupportKHR(device, i, _surface, &presentSupport))
		{
			std::cout << std::format(" ERROR : [ VulkanBase ] failed to get device surface! Error code: {}\n", int32_t(result));
		}

		if (presentSupport)
		{
			indices.PresentFamily = i;
			indices.HasPresentFamily = true;
		}

		if (indices.IsComplete()) break;

		i++;
	}

	return indices;
}

VulkanBase::SwapChainSupportDetails VulkanBase::_query_swap_chain_support(VkPhysicalDevice device)
{
	SwapChainSupportDetails details;
	if (VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, _surface, &details.Capabilities))
	{
		std::cout << std::format("ERROR : [ VulkanBase ] Failed to get physical device surface capabilities! Error code: {}\n", int32_t(result));
		return details;
	}

	uint32_t surfaceFormatCount;
	if (VkResult result = vkGetPhysicalDeviceSurfaceFormatsKHR(device, _surface, &surfaceFormatCount, nullptr))
	{
		std::cout << std::format("ERROR : [ VulkanBase ] Failed to get the count of surface formats! Error code: {}\n", int32_t(result));
		return details;
	}
	if (surfaceFormatCount)
	{
		details.Formats.resize((size_t)surfaceFormatCount);
		if (VkResult result = vkGetPhysicalDeviceSurfaceFormatsKHR(device, _surface, &surfaceFormatCount, details.Formats.data()))
		{
			std::cout << std::format("ERROR : [ VulkanBase ] Failed to get surface formats! Error code: {}\n", int32_t(result));
			details.Formats.clear();
			return details;
		}
	}
	else
	{
		std::cout << std::format("ERROR : [ VulkanBase ] Failed to find any supported surface format!\n");
	}

	uint32_t surfacePresentModeCount;
	if (VkResult result = vkGetPhysicalDeviceSurfacePresentModesKHR(device, _surface, &surfacePresentModeCount, nullptr))
	{
		std::cout << std::format("ERROR : [ VulkanBase ] Failed to get the count of surface present modes! Error code: {}\n", int32_t(result));
		return details;
	}
	if (surfacePresentModeCount)
	{
		details.PresentModes.resize(surfacePresentModeCount);
		if (VkResult result = vkGetPhysicalDeviceSurfacePresentModesKHR(device, _surface, &surfacePresentModeCount, details.PresentModes.data()))
		{
			std::cout << std::format("ERROR : [ VulkanBase ] Failed to get surface present modes!\nError code: {}\n", int32_t(result));
			details.PresentModes.clear();
			return details;
		}
	}
	else
	{
		std::cout << std::format("ERROR : [ VulkanBase ] Failed to find any surface present mode!\n");
	}

	return details;
}

