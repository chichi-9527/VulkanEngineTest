#pragma once

#include "VkShader.h"

#include <vulkan/vulkan.h>

#include <vector>

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

	struct SwapChainSupportDetails {
		VkSurfaceCapabilitiesKHR Capabilities;
		std::vector<VkSurfaceFormatKHR> Formats;
		std::vector<VkPresentModeKHR> PresentModes;
	};

	static VulkanBase& Base();
	uint32_t GetVulkanVersion() const { return _api_version; }
	VkInstance GetVkInstance() const { return _instance; }
	VkDevice GetVkDevice() const { return _device; }
	VkSurfaceKHR GetSurface() const { return _surface; }
	void SetSurface(VkSurfaceKHR surface) { if (!_surface) _surface = surface; }

	// create instance
	bool InitVulkanInstance();
	/// <summary>
	/// 在调用此函数之前必须设置 surface (函数 SetSurface) 或通过子类重载、更改 CreateSurface() 等方式创建 _surface
	/// </summary>
	/// <returns></returns>
	bool InitVulkan();

	void CleanUp() const;

	void AddValidationLayer(const char* layerName);
	void AddInstanceExtension(const char* extensionName);
	void AddDeviceExtension(const char* extensionName);

	/// <summary>
	/// 创建渲染管线
	/// </summary>
	void CreateGraphicsPipeline();

protected:
	virtual bool CreateSurface();

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
	bool _check_device_extension_support(VkPhysicalDevice device);

	bool _pick_physical_device();
	bool _create_logical_device();
	bool _create_swap_chain();
	bool _create_image_views();
	VkSurfaceFormatKHR _choose_swap_surface_format(const std::vector<VkSurfaceFormatKHR>& available_formats);
	/// <summary>
	/// VK_PRESENT_MODE_IMMEDIATE_KHR：通过应用程序提交的图像会立即传输到屏幕上;
	/// VK_PRESENT_MODE_FIFO_KHR : 交换链是一个队列，显示器刷新时，会从队列前端取出图像，程序则将渲染好的图像插入队列后端。如果队列已满，程序就必须等待。
	/// VK_PRESENT_MODE_FIFO_RELAXED_KHR: 此模式与前一种模式的区别仅在于应用程序延迟且在上一个垂直空白时队列为空。此时，图像不会等待下一个垂直空白，而是在最终到达时立即传输。这可能会导致画面出现可见的撕裂。
	/// VK_PRESENT_MODE_MAILBOX_KHR : 当队列满时，应用程序不会阻塞，而是直接用新生成的图像替换已排队的图像。
	/// </summary>
	/// <param name="available_present_modes"></param>
	/// <param name="mode">优先选择，如果不支持总是选择 VK_PRESENT_MODE_FIFO_KHR </param>
	/// <returns></returns>
	VkPresentModeKHR _choose_swap_presenta_mode(const std::vector<VkPresentModeKHR>& available_present_modes, VkPresentModeKHR mode = VK_PRESENT_MODE_MAILBOX_KHR);
	VkExtent2D _choose_swap_extent(const VkSurfaceCapabilitiesKHR& capabilities);
	QueueFamilyIndices _find_queue_families(VkPhysicalDevice device);
	SwapChainSupportDetails _query_swap_chain_support(VkPhysicalDevice device);

public:


private:
	SwapChainSupportDetails _swap_chain_support;
	std::vector<VkImage> _swap_chain_images;
	std::vector<VkImageView> _swap_chain_image_views;

	VkInstance _instance;
	VkDevice _device;
	VkDebugUtilsMessengerEXT _debug_messenger;
	VkPhysicalDevice _physical_device;
	VkQueue _graphics_queue;
	VkQueue _present_queue;
	VkSurfaceKHR _surface;
	VkSwapchainKHR _swap_chain;
	VkFormat _swap_chain_image_format;
	VkExtent2D _swap_chain_extent;

	QueueFamilyIndices _queue_family_indices;

	uint32_t _api_version;

	uint32_t _frame_buffer_width = 0;
	uint32_t _frame_buffer_height = 0;

	

};

