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
	_pick_physical_device();
	_create_logical_device();
	return true;
}

void VulkanBase::CleanUp() const
{
#if defined(UseDebugMessenger) || !defined(NDEBUG)
	DestroyDebugUtilsMessengerEXT(_instance, _debug_messenger, nullptr);
#endif

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
	std::cout << "AddInstanceLayer ： " << layerName << "\n";
	validationLayers.push_back(layerName);
}
void VulkanBase::AddInstanceExtension(const char* extensionName)
{
	std::cout << "AddInstanceExtension ： " << extensionName << "\n";
	instanceExtensions.push_back(extensionName);
}
void VulkanBase::AddDeviceExtension(const char* extensionName)
{
	std::cout << "AddDeviceExtension ： " << extensionName << "\n";
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
	std::cout << std::format("INFO : Check instanceLayers done, Num : {}\n", validationLayers.size());
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

		return deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && deviceFeatures.geometryShader && _queue_family_indices.IsComplete();
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
	deviceCreateInfo.enabledExtensionCount = 0;
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

bool VulkanBase::_create_surface()
{
	return false;
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

