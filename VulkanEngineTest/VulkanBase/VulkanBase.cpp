// 模拟预处理头
#include "VulkanMemoryAllocator/VmaUsage.h"

#include "VulkanBase.h"
#include "Vertex.h"
#include "VkShader.h"

#include <iostream>
#include <format>
#include <vector>
#include <set>
#include <unordered_map>
#include <limits>
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <thread>

constexpr unsigned int MAX_FRAMES_IN_FLIGHT = 2;
static auto RunPath = std::filesystem::current_path().string();
static uint32_t ImageIndex = 0;

static VmaAllocator vmaAllocator = nullptr;
static std::unordered_map<VkBuffer, VmaAllocation> MapBufferAllocation;

const std::vector<Vertex> vertices = {
	{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
	{{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
	{{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
	{{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}
};

const std::vector<uint16_t> indices = {
	0,1,2,2,3,0
};

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

void VulkanBase::CreateVmaAllocator(VkInstance instance, VkDevice device, VkPhysicalDevice physical_device)
{
	VmaVulkanFunctions vulkanFunctions = {};
	vulkanFunctions.vkGetInstanceProcAddr = &vkGetInstanceProcAddr;
	vulkanFunctions.vkGetDeviceProcAddr = &vkGetDeviceProcAddr;

	VmaAllocatorCreateInfo allocatorCreateInfo = {};
	allocatorCreateInfo.flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
	allocatorCreateInfo.vulkanApiVersion = VulkanBase::Base().GetVulkanVersion();
	allocatorCreateInfo.physicalDevice = physical_device;
	allocatorCreateInfo.device = device;
	allocatorCreateInfo.instance = instance;
	allocatorCreateInfo.pVulkanFunctions = &vulkanFunctions;

	vmaCreateAllocator(&allocatorCreateInfo, &vmaAllocator);

	std::cout << std::format("INFO : [VulkanBase] Create Vma Allocator done. \n");
}

void VulkanBase::DestoryVmaAllocator()
{
	vmaDestroyAllocator(vmaAllocator);
}

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
	_create_image_views();
	VulkanBase::CreateVmaAllocator(_instance, _device, _physical_device);
	_create_render_pass();
	_create_graphics_pipeline();
	_create_framebuffers();
	_create_command_pool();
	//_create_vertex_buffer();
	_vma_create_vertex_buffer();
	_vma_create_index_buffer();
	_create_command_buffer();
	_create_sync_objects();
	return true;
}

void VulkanBase::WaitForFence(uint32_t& frameIndex)
{
	if (VkResult result = vkWaitForFences(_device, 1, &_frame_fences[frameIndex], VK_TRUE, UINT64_MAX))
	{
		std::cout << std::format("ERROR : [VulkanBase] vkWaitForFences error : {}\n", (int32_t)result);
	}
	
	/*if (VkResult result = vkResetFences(_device, 1, &_frame_fences[frameIndex]))
	{
		std::cout << std::format("ERROR : [VulkanBase] vkResetFences error : {}\n", (int32_t)result);
	}*/
}

int VulkanBase::AcquireNextImage(uint32_t& frameIndex)
{
	VkResult result = vkAcquireNextImageKHR(_device, _swap_chain, UINT64_MAX, _acquire_semaphores[frameIndex], VK_NULL_HANDLE, &ImageIndex);
	if (result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		_recreate_swap_chain();
		return -1;
	}
	else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
	{
		std::cout << std::format("ERROR : [VulkanBase] failed to acquire swap chain image! \n");
		return -2;
	}

	// 延迟重置围栏
	if (VkResult result = vkResetFences(_device, 1, &_frame_fences[frameIndex]))
	{
		std::cout << std::format("ERROR : [VulkanBase] vkResetFences error : {}\n", (int32_t)result);
	}

	return 0;
}

void VulkanBase::ResetCommandBuffer()
{
	vkResetCommandBuffer(_command_buffer, 0);
}

void VulkanBase::RecordCommandBuffer(uint32_t& frameIndex)
{
	_record_command_buffer(ImageIndex);
}

bool VulkanBase::SubmitCommandBuffer(uint32_t& frameIndex)
{
	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkSemaphore waitSemaphores[] = { _acquire_semaphores[frameIndex]};
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &_command_buffer;

	VkSemaphore signalSemaphores[] = { _submit_semaphores[ImageIndex]};
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;

	if(VkResult result = vkQueueSubmit(_graphics_queue, 1, &submitInfo, _frame_fences[frameIndex]))
	{
		std::cout << std::format("ERROR : failed to submit draw command buffer! error code : {} \n", int32_t(result));
		return false;
	}

	return true;
}

void VulkanBase::Present(uint32_t& frameIndex)
{
	VkSemaphore signalSemaphores[] = { _submit_semaphores[ImageIndex] };
	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;

	VkSwapchainKHR swapChains[] = { _swap_chain };
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &ImageIndex;
	presentInfo.pResults = nullptr;

	VkResult result = vkQueuePresentKHR(_present_queue, &presentInfo);
	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || _framebuffer_resized)
	{
		_framebuffer_resized = false;
		_recreate_swap_chain();
	}
	else if (result != VK_SUCCESS)
	{
		std::cout << std::format("ERROR : failed to present swap chain image! error code : {} \n", int32_t(result));
	}
}

void VulkanBase::WaitIdle()
{
	vkDeviceWaitIdle(_device);
}

bool VulkanBase::CreateBuffer(VkDeviceSize size,
	VkBufferUsageFlags usage,
	VkMemoryPropertyFlags properties,
	VkBuffer& buffer,
	VkDeviceMemory& buffer_memory)
{
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (VkResult result = vkCreateBuffer(_device, &bufferInfo, nullptr, &buffer))
	{
		std::cout << std::format("ERROR : [ VulkanBase ] Failed to create buffer! VkBufferUsageFlags : {},  Error code: {}\n", int32_t(usage), int32_t(result));
		return false;
	}

	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(_device, buffer, &memRequirements);

	auto findMemoryType = [this](uint32_t type_filter, VkMemoryPropertyFlags properties) -> uint32_t {
		VkPhysicalDeviceMemoryProperties memProperties;
		vkGetPhysicalDeviceMemoryProperties(_physical_device, &memProperties);

		for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
		{
			if ((type_filter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
			{
				return i;
			}
		}

		std::cout << std::format("ERROR : [ VulkanBase ] failed to find suitable memory type!\n");
		return (uint32_t)-1;
		};

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

	if (VkResult result = vkAllocateMemory(_device, &allocInfo, nullptr, &buffer_memory))
	{
		std::cout << std::format("ERROR : [ VulkanBase ] Failed to allocate vertex buffer memory! Error code: {}\n", int32_t(result));
		return false;
	}

	vkBindBufferMemory(_device, buffer, buffer_memory, 0);

	return true;
}

bool VulkanBase::UseVmaCreateBuffer(VkDeviceSize size,
	VkBufferUsageFlags usage, 
	VkMemoryPropertyFlags properties,
	VkBuffer& buffer)
{
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo vmaAllocInfo{};
	vmaAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
	if (properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
	{
		vmaAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
	}

	VmaAllocation allocation;
	if (VkResult result = vmaCreateBuffer(vmaAllocator, &bufferInfo, &vmaAllocInfo, &buffer, &allocation, nullptr))
	{
		std::cout << std::format("ERROR : [ VulkanBase ] Failed to create buffer! VkBufferUsageFlags : {},  Error code: {}\n", int32_t(usage), int32_t(result));
		return false;
	}

	MapBufferAllocation[buffer] = allocation;

	return true;
}

void VulkanBase::UseVmaDestroyBuffer(VkBuffer buffer)
{
	auto iter = MapBufferAllocation.find(buffer);
	if (iter != MapBufferAllocation.end())
		vmaDestroyBuffer(vmaAllocator, buffer, iter->second);
	MapBufferAllocation.erase(buffer);
}

bool VulkanBase::CopyBuffer(VkBuffer src_buffer, VkBuffer dst_buffer, VkDeviceSize size)
{
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = _command_pool;
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer;
	vkAllocateCommandBuffers(_device, &allocInfo, &commandBuffer);

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(commandBuffer, &beginInfo);

	VkBufferCopy copyRegion{};
	copyRegion.srcOffset = 0;
	copyRegion.dstOffset = 0;
	copyRegion.size = size;
	vkCmdCopyBuffer(commandBuffer, src_buffer, dst_buffer, 1, &copyRegion);

	vkEndCommandBuffer(commandBuffer);

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	vkQueueSubmit(_graphics_queue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(_graphics_queue); // vkWaitForFences

	vkFreeCommandBuffers(_device, _command_pool, 1, &commandBuffer);

	return true;
}

//void VulkanBase::DrawFrame()
//{
//	vkWaitForFences(_device, 1, &_in_flight_fence, VK_TRUE, UINT64_MAX);
//	//std::this_thread::sleep_for(std::chrono::milliseconds(1000));
//	vkResetFences(_device, 1, &_in_flight_fence);
//
//	uint32_t imageIndex;
//	vkAcquireNextImageKHR(_device, _swap_chain, UINT64_MAX, _image_available_semaphore, VK_NULL_HANDLE, &imageIndex);
//
//	vkResetCommandBuffer(_command_buffer, /*VkCommandBufferResetFlagBits*/ 0);
//	_record_command_buffer(imageIndex);
//
//	VkSubmitInfo submitInfo{};
//	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
//
//	VkSemaphore waitSemaphores[] = { _image_available_semaphore };
//	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
//	submitInfo.waitSemaphoreCount = 1;
//	submitInfo.pWaitSemaphores = waitSemaphores;
//	submitInfo.pWaitDstStageMask = waitStages;
//
//	submitInfo.commandBufferCount = 1;
//	submitInfo.pCommandBuffers = &_command_buffer;
//
//	VkSemaphore signalSemaphores[] = { _render_finished_semaphore };
//	submitInfo.signalSemaphoreCount = 1;
//	submitInfo.pSignalSemaphores = signalSemaphores;
//
//	if (vkQueueSubmit(_graphics_queue, 1, &submitInfo, _in_flight_fence) != VK_SUCCESS)
//	{
//		throw std::runtime_error("failed to submit draw command buffer!");
//	}
//
//	VkPresentInfoKHR presentInfo{};
//	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
//
//	presentInfo.waitSemaphoreCount = 1;
//	presentInfo.pWaitSemaphores = signalSemaphores;
//
//	VkSwapchainKHR swapChains[] = { _swap_chain };
//	presentInfo.swapchainCount = 1;
//	presentInfo.pSwapchains = swapChains;
//
//	presentInfo.pImageIndices = &imageIndex;
//
//	vkQueuePresentKHR(_present_queue, &presentInfo);
//}

void VulkanBase::CleanUp()
{
#if defined(UseDebugMessenger) || !defined(NDEBUG)
	DestroyDebugUtilsMessengerEXT(_instance, _debug_messenger, nullptr);
#endif
	/*vkDestroySemaphore(_device, _image_available_semaphore, nullptr);
	vkDestroySemaphore(_device, _render_finished_semaphore, nullptr);
	vkDestroyFence(_device, _in_flight_fence, nullptr);*/

	for (auto& semaphore : _acquire_semaphores)
	{
		vkDestroySemaphore(_device, semaphore, nullptr);
	}
	for (auto& semaphore : _submit_semaphores)
	{
		vkDestroySemaphore(_device, semaphore, nullptr);
	}
	for (auto& fence : _frame_fences)
	{
		vkDestroyFence(_device, fence, nullptr);
	}

	vkDestroyCommandPool(_device, _command_pool, nullptr);

	for(auto framebuffer : _swap_chain_framebuffers)
	{
		vkDestroyFramebuffer(_device, framebuffer, nullptr);
	}

	//vkDestroyBuffer(_device, _vertex_buffer, nullptr);
	UseVmaDestroyBuffer(_vertex_buffer);
	//vkFreeMemory(_device, _vertex_buffer_memory, nullptr);
	UseVmaDestroyBuffer(_index_buffer);
	
	vkDestroyPipeline(_device, _graphics_pipeline, nullptr);
	vkDestroyPipelineLayout(_device, _pipeline_layout, nullptr);
	vkDestroyRenderPass(_device, _render_pass, nullptr);

	VulkanBase::DestoryVmaAllocator();

	for (auto imageView : _swap_chain_image_views)
	{
		vkDestroyImageView(_device, imageView, nullptr);
	}

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

void VulkanBase::FrameBufferResize(uint32_t width, uint32_t height)
{
	_frame_buffer_width = width;
	_frame_buffer_height = height;
	this->_framebuffer_resized = true;
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

	VkPhysicalDeviceVulkan11Features feat11 = {};
	feat11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
	feat11.shaderDrawParameters = VK_TRUE;

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
	deviceCreateInfo.pNext = &feat11;
	

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
	
	// 获取最新的 Surface Capabilities
	if (VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(_physical_device, _surface, &_swap_chain_support.Capabilities))
	{
		std::cout << std::format("ERROR : [ VulkanBase ] Failed to Get Physical Device Surface Capabilities! Error code: {}\n", int32_t(result));
		return false;
	}
	auto extent = _choose_swap_extent(_swap_chain_support.Capabilities);

	uint32_t imageCount = _swap_chain_support.Capabilities.minImageCount + 1;
	if (_swap_chain_support.Capabilities.maxImageCount > 0 && imageCount > _swap_chain_support.Capabilities.maxImageCount)
	{
		imageCount = _swap_chain_support.Capabilities.maxImageCount;
	}

	// VkSwapchainCreateInfoKHR
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
	_swap_chain_image_format = surfaceFormat.format;
	_swap_chain_extent = extent;
	_swap_chain_image_count = swapchainImageCount;

	return true;
}

bool VulkanBase::_recreate_swap_chain()
{
	while (_frame_buffer_width == 0 || _frame_buffer_height == 0)
	{
		
	}

	vkDeviceWaitIdle(_device);

	if (!_cleanup_swap_chain()) return false;

	if (!_create_swap_chain()) return false;
	if (!_create_image_views()) return false;
	if (!_create_framebuffers()) return false;

	return true;
}

bool VulkanBase::_cleanup_swap_chain()
{
	for (auto framebuffer : _swap_chain_framebuffers)
	{
		vkDestroyFramebuffer(_device, framebuffer, nullptr);
	}

	for (auto imageView : _swap_chain_image_views)
	{
		vkDestroyImageView(_device, imageView, nullptr);
	}

	vkDestroySwapchainKHR(_device, _swap_chain, nullptr);

	return true;
}

bool VulkanBase::_create_image_views()
{
	_swap_chain_image_views.resize(_swap_chain_images.size());
	for (size_t i = 0; i < _swap_chain_images.size(); ++i)
	{
		VkImageViewCreateInfo imageViewCreateInfo{};
		imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCreateInfo.image = _swap_chain_images[i];
		imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;// 1D 纹理、2D 纹理、3D 纹理和立方体贴图。
		imageViewCreateInfo.format = _swap_chain_image_format;
		imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
		imageViewCreateInfo.subresourceRange.levelCount = 1;
		imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
		imageViewCreateInfo.subresourceRange.layerCount = 1;

		if (VkResult result = vkCreateImageView(_device, &imageViewCreateInfo, nullptr, &_swap_chain_image_views[i]))
		{
			std::cout << std::format("ERROR : [ VulkanBase ] Failed to Create ImageView! Error code: {}\n", int32_t(result));
			return false;
		}
	}
	return true;
}

bool VulkanBase::_create_render_pass()
{
	VkAttachmentDescription colorAttachment{};
	colorAttachment.format = _swap_chain_image_format;
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

	VkSubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = 1;
	renderPassInfo.pAttachments = &colorAttachment;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;

	if (VkResult result = vkCreateRenderPass(_device, &renderPassInfo, nullptr, &_render_pass))
	{
		std::cout << std::format("ERROR : [ VulkanBase ] Failed to create render pass! Error code: {}\n", int32_t(result));
		return false;
	}

	return true;
}

bool VulkanBase::_create_graphics_pipeline()
{
	auto vert_path = RunPath + "\\shader\\vulkan\\SPV\\fristTriangle.slang.vert.spv";
	VkEngineShaderModule vertShaderModule(_device, vert_path);
	auto frag_path = RunPath + "\\shader\\vulkan\\SPV\\fristTriangle.slang.frag.spv";
	VkEngineShaderModule fragShaderModule(_device, frag_path);

	VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = vertShaderModule.GetShaderModule();
	vertShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = fragShaderModule.GetShaderModule();
	fragShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

	// add vertex
	auto bindingDescription = Vertex::getBindingDescription();
	auto attributeDescriptions = Vertex::getAttributeDescriptions();
	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
	vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
	vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	std::vector<VkDynamicState> dynamicStates = {
	VK_DYNAMIC_STATE_VIEWPORT,
	VK_DYNAMIC_STATE_SCISSOR
	};

	VkPipelineDynamicStateCreateInfo dynamicState{};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicState.pDynamicStates = dynamicStates.data();

	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;

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
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
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

	if (VkResult result = vkCreatePipelineLayout(_device, &pipelineLayoutInfo, nullptr, &_pipeline_layout))
	{
		std::cout << std::format("ERROR : [ VulkanBase ] Failed to create pipeline layout! Error code: {}\n", int32_t(result));
		return false;
	}

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
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.layout = _pipeline_layout;
	pipelineInfo.renderPass = _render_pass;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex = -1;

	if (VkResult result = vkCreateGraphicsPipelines(_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &_graphics_pipeline))
	{
		std::cout << std::format("ERROR : [ VulkanBase ] Failed to create graphics pipeline! Error code: {}\n", int32_t(result));
		return false;
	}


	// VkEngineShaderModule 析构时会自动调用 vkDestroyShaderModule 释放资源
	return true;
}

bool VulkanBase::_create_vertex_buffer()
{
	
	VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	if (!CreateBuffer(bufferSize, 
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
		stagingBuffer, 
		stagingBufferMemory))
	{
		return false;
	}

	void* data;
	vkMapMemory(_device, stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, vertices.data(), (size_t)bufferSize);
	vkUnmapMemory(_device, stagingBufferMemory);

	if (!CreateBuffer(bufferSize, 
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		_vertex_buffer, 
		_vertex_buffer_memory))
	{
		return false;
	}

	CopyBuffer(stagingBuffer, _vertex_buffer, bufferSize);

	vkDestroyBuffer(_device, stagingBuffer, nullptr);
	vkFreeMemory(_device, stagingBufferMemory, nullptr);

	return true;
}

bool VulkanBase::_vma_create_vertex_buffer()
{
	VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

	VkBuffer stagingBuffer;

	if (!UseVmaCreateBuffer(bufferSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBuffer))
	{
		return false;
	}

	void* data;
	vmaMapMemory(vmaAllocator, MapBufferAllocation[stagingBuffer], &data);
	memcpy(data, vertices.data(), (size_t)bufferSize);
	vmaUnmapMemory(vmaAllocator, MapBufferAllocation[stagingBuffer]);

	if (!UseVmaCreateBuffer(bufferSize,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		_vertex_buffer))
	{
		return false;
	}

	CopyBuffer(stagingBuffer, _vertex_buffer, bufferSize);

	UseVmaDestroyBuffer(stagingBuffer);

	return true;
}

bool VulkanBase::_vma_create_index_buffer()
{
	VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

	VkBuffer stagingBuffer;
	if (!UseVmaCreateBuffer(bufferSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBuffer))
	{
		return false;
	}

	void* data;
	vmaMapMemory(vmaAllocator, MapBufferAllocation[stagingBuffer], &data);
	memcpy(data, indices.data(), (size_t)bufferSize);
	vmaUnmapMemory(vmaAllocator, MapBufferAllocation[stagingBuffer]);

	if (!UseVmaCreateBuffer(bufferSize,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		_index_buffer))
	{
		return false;
	}

	CopyBuffer(stagingBuffer, _index_buffer, bufferSize);

	UseVmaDestroyBuffer(stagingBuffer);

	return true;
}

bool VulkanBase::_create_framebuffers()
{
	_swap_chain_framebuffers.resize(_swap_chain_image_views.size());

	for(size_t i = 0; i < _swap_chain_image_views.size(); ++i)
	{
		VkImageView attachments[] = {
			_swap_chain_image_views[i]
		};
		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = _render_pass;
		framebufferInfo.attachmentCount = 1;
		framebufferInfo.pAttachments = attachments;
		framebufferInfo.width = _swap_chain_extent.width;
		framebufferInfo.height = _swap_chain_extent.height;
		framebufferInfo.layers = 1;
		if (VkResult result = vkCreateFramebuffer(_device, &framebufferInfo, nullptr, &_swap_chain_framebuffers[i]))
		{
			std::cout << std::format("ERROR : [ VulkanBase ] Failed to create framebuffer! Error code: {}\n", int32_t(result));
			return false;
		}
	}

	return true;
}

bool VulkanBase::_create_command_pool()
{
	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	poolInfo.queueFamilyIndex = _queue_family_indices.GraphicsFamily;
	if (VkResult result = vkCreateCommandPool(_device, &poolInfo, nullptr, &_command_pool))
	{
		std::cout << std::format("ERROR : [ VulkanBase ] Failed to create command pool! Error code: {}\n", int32_t(result));
		return false;
	}

	return true;
}

bool VulkanBase::_create_command_buffer()
{
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = _command_pool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = 1;

	if (VkResult result = vkAllocateCommandBuffers(_device, &allocInfo, &_command_buffer))
	{
		std::cout << std::format("ERROR : [ VulkanBase ] Failed to allocate command buffer! Error code: {}\n", int32_t(result));
		return false;
	}

	return true;
}

bool VulkanBase::_record_command_buffer(uint32_t imageIndex)
{
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = 0;
	beginInfo.pInheritanceInfo = nullptr;

	if (VkResult result = vkBeginCommandBuffer(_command_buffer, &beginInfo))
	{
		std::cout << std::format("ERROR : [ VulkanBase ] Failed to begin recording command buffer! Error code: {}\n", int32_t(result));
		return false;
	}

	VkRenderPassBeginInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = _render_pass;
	renderPassInfo.framebuffer = _swap_chain_framebuffers[imageIndex];
	renderPassInfo.renderArea.offset = { 0, 0 };
	renderPassInfo.renderArea.extent = _swap_chain_extent;

	VkClearValue clearColor = { {{0.0f, 0.0f, 0.0f, 1.0f}} };
	renderPassInfo.clearValueCount = 1;
	renderPassInfo.pClearValues = &clearColor;

	vkCmdBeginRenderPass(_command_buffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
	vkCmdBindPipeline(_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _graphics_pipeline);

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(_swap_chain_extent.width);
	viewport.height = static_cast<float>(_swap_chain_extent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(_command_buffer, 0, 1, &viewport);

	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = _swap_chain_extent;
	vkCmdSetScissor(_command_buffer, 0, 1, &scissor);

	// 
	VkBuffer vertexBuffers[] = { _vertex_buffer };
	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers(_command_buffer, 0, 1, vertexBuffers, offsets);

	vkCmdBindIndexBuffer(_command_buffer, _index_buffer, 0, VK_INDEX_TYPE_UINT16);

	//vkCmdDraw(_command_buffer, 3, 1, 0, 0);
	vkCmdDrawIndexed(_command_buffer, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);
	vkCmdEndRenderPass(_command_buffer);

	if (VkResult result = vkEndCommandBuffer(_command_buffer))
	{
		std::cout << std::format("ERROR : [ VulkanBase ] Failed to record command buffer! Error code: {}\n", int32_t(result));
		return false;
	}

	return true;
}

bool VulkanBase::_create_sync_objects()
{
	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // 初始状态为已信号，避免第一次等待时死锁

	_frame_fences.resize(MAX_FRAMES_IN_FLIGHT);
	for (unsigned int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
	{
		if (VkResult result = vkCreateFence(_device, &fenceInfo, nullptr, &_frame_fences[i]))
		{
			std::cout << std::format("ERROR : [ VulkanBase ] Failed to create fences! Error codes: {},\n", int32_t(result));
			return false;
		}
	}
	_acquire_semaphores.resize(MAX_FRAMES_IN_FLIGHT);
	for (unsigned int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
	{
		if (VkResult result = vkCreateSemaphore(_device, &semaphoreInfo, nullptr, &_acquire_semaphores[i]))
		{
			std::cout << std::format("ERROR : [ VulkanBase ] Failed to create _acquire_semaphores! Error codes: {},\n", int32_t(result));
			return false;
		}
	}
	_submit_semaphores.resize(_swap_chain_image_count);
	for (unsigned int i = 0; i < _swap_chain_image_count; ++i)
	{
		if (VkResult result = vkCreateSemaphore(_device, &semaphoreInfo, nullptr, &_submit_semaphores[i]))
		{
			std::cout << std::format("ERROR : [ VulkanBase ] Failed to create _submit_semaphores! Error codes: {},\n", int32_t(result));
			return false;
		}
	}

	/*VkResult imgSemaphoreResult = vkCreateSemaphore(_device, &semaphoreInfo, nullptr, &_image_available_semaphore);
	VkResult renderSemaphoreResult = vkCreateSemaphore(_device, &semaphoreInfo, nullptr, &_render_finished_semaphore);
	VkResult fenceResult = vkCreateFence(_device, &fenceInfo, nullptr, &_in_flight_fence);
	if (imgSemaphoreResult != VK_SUCCESS || renderSemaphoreResult != VK_SUCCESS || fenceResult != VK_SUCCESS)
	{
		std::cout << std::format("ERROR : [ VulkanBase ] Failed to create synchronization objects! Error codes: {}, {}, {}\n",
			int32_t(imgSemaphoreResult), int32_t(renderSemaphoreResult), int32_t(fenceResult));

		return false;
	}*/

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

