#pragma once

#include <vulkan/vulkan.h>

#ifndef UseDebugMessenger
#define UseDebugMessenger
#endif

class VulkanBase
{
public:
	~VulkanBase();

	struct QueueFamilyIndices {
		uint32_t GraphicsFamily = 0;
		uint32_t PresentFamily = 0;

		bool HasGraphicsFamily = false;
		bool HasPresentFamily = false;

		bool IsComplete() const
		{
			return HasGraphicsFamily && HasPresentFamily;
		}
	};

	static VulkanBase& Base();
	uint32_t GetVulkanVersion() const { return _api_version; }
	VkInstance GetVkInstance() const { return _instance; }
	VkDevice GetVkDevice() const { return _device; }
	VkSurfaceKHR GetSurface() const { return _surface; }
	void SetSurface(VkSurfaceKHR surface) { if (!_surface) _surface = surface; }

	// create instance
	bool InitVulkanInstance();
	bool InitVulkan();

	void CleanUp() const;

	void AddValidationLayer(const char* layerName);
	void AddInstanceExtension(const char* extensionName);
	void AddDeviceExtension(const char* extensionName);

	

private:
	VulkanBase();

	bool _create_instance();
	void _use_lastest_api_version();
	bool _setup_debug_messenger();
#ifdef UseDebugMessenger
	bool _setup_debug_messenger_in_release();
#endif	// UseDebugMessenger
	// 检查验证层
	bool _check_validation_layers();

	bool _pick_physical_device();
	bool _create_logical_device();
	bool _create_surface();
	QueueFamilyIndices _find_queue_families(VkPhysicalDevice device);

public:


private:
	VkInstance _instance;
	VkDevice _device;
	VkDebugUtilsMessengerEXT _debug_messenger;
	VkPhysicalDevice _physical_device;
	VkQueue _graphics_queue;
	VkQueue _present_queue;
	VkSurfaceKHR _surface;

	QueueFamilyIndices _queue_family_indices;
	uint32_t _api_version;

};

