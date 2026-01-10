#pragma once

#include <vector>
#include <span>
#include <iostream>
#include <format>
#include <istream>
#include <fstream>

#ifndef GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#endif // !GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#ifdef _WIN32                        //考虑平台是Windows的情况（请自行解决其他平台上的差异）
#define VK_USE_PLATFORM_WIN32_KHR    //在包含vulkan.h前定义该宏，会一并包含vulkan_win32.h和windows.h
#define NOMINMAX                     //定义该宏可避免windows.h中的min和max两个宏与标准库中的函数名冲突
#endif
#include <vulkan/vulkan.h>

constexpr VkExtent2D defaultWindowSize = { 1280, 720 };

class graphicsBase {
    uint32_t apiVersion = VK_API_VERSION_1_0;
    VkInstance instance;
    VkPhysicalDevice physicalDevice;
    VkPhysicalDeviceProperties physicalDeviceProperties;
    VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties;
    std::vector<VkPhysicalDevice> availablePhysicalDevices;

    VkDevice device;
    uint32_t queueFamilyIndex_graphics = VK_QUEUE_FAMILY_IGNORED;
    uint32_t queueFamilyIndex_presentation = VK_QUEUE_FAMILY_IGNORED;
    uint32_t queueFamilyIndex_compute = VK_QUEUE_FAMILY_IGNORED;
    VkQueue queue_graphics;
    VkQueue queue_presentation;
    VkQueue queue_compute;

    VkSurfaceKHR surface;
    std::vector <VkSurfaceFormatKHR> availableSurfaceFormats;

    VkSwapchainKHR swapchain;
    std::vector <VkImage> swapchainImages;
    std::vector <VkImageView> swapchainImageViews;
    VkSwapchainCreateInfoKHR swapchainCreateInfo = {};


    std::vector<const char*> instanceLayers;
    std::vector<const char*> instanceExtensions;
    std::vector<const char*> deviceExtensions;

    std::vector<void(*)()> callbacks_createDevice;
    std::vector<void(*)()> callbacks_destroyDevice;
    std::vector<void(*)()> callbacks_createSwapchain;
    std::vector<void(*)()> callbacks_destroySwapchain;

    //当前取得的交换链图像索引
    uint32_t currentImageIndex = 0;

    VkDebugUtilsMessengerEXT debugMessenger;

    //静态变量
    static graphicsBase singleton;
    //--------------------
    graphicsBase() = default;
    graphicsBase(graphicsBase&&) = delete;

    ~graphicsBase()
    {
        if (!instance)
            return;
        if (device)
        {
            WaitIdle();
            if (swapchain)
            {
                ExecuteCallbacks(callbacks_destroySwapchain);
                for (auto& i : swapchainImageViews)
                    if (i)
                        vkDestroyImageView(device, i, nullptr);
                vkDestroySwapchainKHR(device, swapchain, nullptr);
            }
            ExecuteCallbacks(callbacks_destroyDevice);
            vkDestroyDevice(device, nullptr);
        }
        if (surface)
            vkDestroySurfaceKHR(instance, surface, nullptr);
        if (debugMessenger)
        {
            PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessenger =
                reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
            if (vkDestroyDebugUtilsMessenger)
                vkDestroyDebugUtilsMessenger(instance, debugMessenger, nullptr);
        }
        vkDestroyInstance(instance, nullptr);
    }

    // Non - const函数
    VkResult CreateSwapchain_Internal()
    {
        if (VkResult result = vkCreateSwapchainKHR(device, &swapchainCreateInfo, nullptr, &swapchain))
        {
            std::cout << std::format("[ graphicsBase ] ERROR\nFailed to create a swapchain!\nError code: {}\n", int32_t(result));
            return result;
        }

        //获取交换连图像
        uint32_t swapchainImageCount;
        if (VkResult result = vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, nullptr))
        {
            std::cout << std::format("[ graphicsBase ] ERROR\nFailed to get the count of swapchain images!\nError code: {}\n", int32_t(result));
            return result;
        }
        swapchainImages.resize(swapchainImageCount);
        if (VkResult result = vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, swapchainImages.data()))
        {
            std::cout << std::format("[ graphicsBase ] ERROR\nFailed to get swapchain images!\nError code: {}\n", int32_t(result));
            return result;
        }

        //创建image view
        swapchainImageViews.resize(swapchainImageCount);
        VkImageViewCreateInfo imageViewCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = swapchainCreateInfo.imageFormat,
            //.components = {}, //四个成员皆为VK_COMPONENT_SWIZZLE_IDENTITY
            .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
        };
        for (size_t i = 0; i < swapchainImageCount; i++)
        {
            imageViewCreateInfo.image = swapchainImages[i];
            if (VkResult result = vkCreateImageView(device, &imageViewCreateInfo, nullptr, &swapchainImageViews[i]))
            {
                std::cout << std::format("[ graphicsBase ] ERROR\nFailed to create a swapchain image view!\nError code: {}\n", int32_t(result));
                return result;
            }
        }
        return VK_SUCCESS;
    }
    VkResult GetQueueFamilyIndices(VkPhysicalDevice physicalDevice, bool enableGraphicsQueue, bool enableComputeQueue, uint32_t(&queueFamilyIndices)[3])
    {
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
        if (!queueFamilyCount)
            return VK_RESULT_MAX_ENUM;
        std::vector<VkQueueFamilyProperties> queueFamilyPropertieses(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilyPropertieses.data());
        auto& [ig, ip, ic] = queueFamilyIndices;
        ig = ip = ic = VK_QUEUE_FAMILY_IGNORED;
        for (uint32_t i = 0; i < queueFamilyCount; i++)
        {
            //这三个VkBool32变量指示是否可获取（指应该被获取且能获取）相应队列族索引
            VkBool32
                //只在enableGraphicsQueue为true时获取支持图形操作的队列族的索引
                supportGraphics = bool(enableGraphicsQueue && queueFamilyPropertieses[i].queueFlags & VK_QUEUE_GRAPHICS_BIT),
                supportPresentation = false,
                //只在enableComputeQueue为true时获取支持计算的队列族的索引
                supportCompute = bool(enableComputeQueue && queueFamilyPropertieses[i].queueFlags & VK_QUEUE_COMPUTE_BIT);
            //只在创建了window surface时获取支持呈现的队列族的索引
            if (surface)
                if (VkResult result = vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &supportPresentation))
                {
                    std::cout << std::format("[ graphicsBase ] ERROR\nFailed to determine if the queue family supports presentation!\nError code: {}\n", int32_t(result));
                    return result;
                }
            //若某队列族同时支持图形操作和计算
            if (supportGraphics && supportCompute)
            {
                //若需要呈现，最好是三个队列族索引全部相同
                if (supportPresentation)
                {
                    ig = ip = ic = i;
                    break;
                }
                //除非ig和ic都已取得且相同，否则将它们的值覆写为i，以确保两个队列族索引相同
                if (ig != ic ||
                    ig == VK_QUEUE_FAMILY_IGNORED)
                    ig = ic = i;
                //如果不需要呈现，那么已经可以break了
                if (!surface)
                    break;
            }
            //若任何一个队列族索引可以被取得但尚未被取得，将其值覆写为i
            if (supportGraphics &&
                ig == VK_QUEUE_FAMILY_IGNORED)
                ig = i;
            if (supportPresentation &&
                ip == VK_QUEUE_FAMILY_IGNORED)
                ip = i;
            if (supportCompute &&
                ic == VK_QUEUE_FAMILY_IGNORED)
                ic = i;
        }
        if (ig == VK_QUEUE_FAMILY_IGNORED && enableGraphicsQueue ||
            ip == VK_QUEUE_FAMILY_IGNORED && surface ||
            ic == VK_QUEUE_FAMILY_IGNORED && enableComputeQueue)
            return VK_RESULT_MAX_ENUM;
        queueFamilyIndex_graphics = ig;
        queueFamilyIndex_presentation = ip;
        queueFamilyIndex_compute = ic;
        return VK_SUCCESS;
    }

    VkResult CreateDebugMessenger()
    {
        static PFN_vkDebugUtilsMessengerCallbackEXT DebugUtilsMessengerCallback = [](
            VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
            VkDebugUtilsMessageTypeFlagsEXT messageTypes,
            const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
            void* pUserData)->VkBool32 {
                std::cout << std::format("{}\n\n", pCallbackData->pMessage);
                return VK_FALSE;
            };
        VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .messageSeverity =
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            .messageType =
                VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            .pfnUserCallback = DebugUtilsMessengerCallback
        };
        PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessenger =
            reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
        if (vkCreateDebugUtilsMessenger)
        {
            VkResult result = vkCreateDebugUtilsMessenger(instance, &debugUtilsMessengerCreateInfo, nullptr, &debugMessenger);
            if (result)
                std::cout << std::format("[ graphicsBase ] ERROR\nFailed to create a debug messenger!\nError code: {}\n", int32_t(result));
            return result;
        }
        std::cout << std::format("[ graphicsBase ] ERROR\nFailed to get the function pointer of vkCreateDebugUtilsMessengerEXT!\n");
        return VK_RESULT_MAX_ENUM;
    }
public:
    //Getter
    uint32_t ApiVersion() const
    {
        return apiVersion;
    }
    VkInstance Instance() const
    {
        return instance;
    }
    VkPhysicalDevice PhysicalDevice() const
    {
        return physicalDevice;
    }
    const VkPhysicalDeviceProperties& PhysicalDeviceProperties() const
    {
        return physicalDeviceProperties;
    }
    const VkPhysicalDeviceMemoryProperties& PhysicalDeviceMemoryProperties() const
    {
        return physicalDeviceMemoryProperties;
    }
    VkPhysicalDevice AvailablePhysicalDevice(uint32_t index) const
    {
        return availablePhysicalDevices[index];
    }
    uint32_t AvailablePhysicalDeviceCount() const
    {
        return uint32_t(availablePhysicalDevices.size());
    }

    VkDevice Device() const
    {
        return device;
    }
    uint32_t QueueFamilyIndex_Graphics() const
    {
        return queueFamilyIndex_graphics;
    }
    uint32_t QueueFamilyIndex_Presentation() const
    {
        return queueFamilyIndex_presentation;
    }
    uint32_t QueueFamilyIndex_Compute() const
    {
        return queueFamilyIndex_compute;
    }
    VkQueue Queue_Graphics() const
    {
        return queue_graphics;
    }
    VkQueue Queue_Presentation() const
    {
        return queue_presentation;
    }
    VkQueue Queue_Compute() const
    {
        return queue_compute;
    }

    VkSurfaceKHR Surface() const
    {
        return surface;
    }
    const VkFormat& AvailableSurfaceFormat(uint32_t index) const
    {
        return availableSurfaceFormats[index].format;
    }
    const VkColorSpaceKHR& AvailableSurfaceColorSpace(uint32_t index) const
    {
        return availableSurfaceFormats[index].colorSpace;
    }
    uint32_t AvailableSurfaceFormatCount() const
    {
        return uint32_t(availableSurfaceFormats.size());
    }

    VkSwapchainKHR Swapchain() const
    {
        return swapchain;
    }
    VkImage SwapchainImage(uint32_t index) const
    {
        return swapchainImages[index];
    }
    VkImageView SwapchainImageView(uint32_t index) const
    {
        return swapchainImageViews[index];
    }
    uint32_t SwapchainImageCount() const
    {
        return uint32_t(swapchainImages.size());
    }
    const VkSwapchainCreateInfoKHR& SwapchainCreateInfo() const
    {
        return swapchainCreateInfo;
    }

    const std::vector<const char*>& InstanceLayers() const
    {
        return instanceLayers;
    }
    const std::vector<const char*>& InstanceExtensions() const
    {
        return instanceExtensions;
    }
    const std::vector<const char*>& DeviceExtensions() const
    {
        return deviceExtensions;
    }

    //Const函数
    VkResult CheckInstanceLayers(std::span<const char*> layersToCheck) const
    {
        uint32_t layerCount;
        std::vector<VkLayerProperties> availableLayers;
        if (VkResult result = vkEnumerateInstanceLayerProperties(&layerCount, nullptr))
        {
            std::cout << std::format("[ graphicsBase ] ERROR\nFailed to get the count of instance layers!\n");
            return result;
        }
        if (layerCount)
        {
            availableLayers.resize(layerCount);
            if (VkResult result = vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data()))
            {
                std::cout << std::format("[ graphicsBase ] ERROR\nFailed to enumerate instance layer properties!\nError code: {}\n", int32_t(result));
                return result;
            }
            for (auto& i : layersToCheck)
            {
                bool found = false;
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
            for (auto& i : layersToCheck)
                i = nullptr;
        //一切顺利则返回VK_SUCCESS
        return VK_SUCCESS;
    }
    VkResult CheckInstanceExtensions(std::span<const char*> extensionsToCheck, const char* layerName = nullptr) const
    {
        uint32_t extensionCount;
        std::vector<VkExtensionProperties> availableExtensions;
        if (VkResult result = vkEnumerateInstanceExtensionProperties(layerName, &extensionCount, nullptr))
        {
            layerName ?
                std::cout << std::format("[ graphicsBase ] ERROR\nFailed to get the count of instance extensions!\nLayer name:{}\n", layerName) :
                std::cout << std::format("[ graphicsBase ] ERROR\nFailed to get the count of instance extensions!\n");
            return result;
        }
        if (extensionCount)
        {
            availableExtensions.resize(extensionCount);
            if (VkResult result = vkEnumerateInstanceExtensionProperties(layerName, &extensionCount, availableExtensions.data()))
            {
                std::cout << std::format("[ graphicsBase ] ERROR\nFailed to enumerate instance extension properties!\nError code: {}\n", int32_t(result));
                return result;
            }
            for (auto& i : extensionsToCheck)
            {
                bool found = false;
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
            for (auto& i : extensionsToCheck)
                i = nullptr;
        return VK_SUCCESS;
    }
    VkResult CheckDeviceExtensions(std::span<const char*> extensionsToCheck, const char* layerName = nullptr) const
    {
        /*待Ch1-3填充*/
    }

    //Non-const函数
    void AddInstanceLayer(const char* layerName)
    {
        instanceLayers.push_back(layerName);
    }
    void AddInstanceExtension(const char* extensionName)
    {
        instanceExtensions.push_back(extensionName);
    }
    void AddDeviceExtension(const char* extensionName)
    {
        deviceExtensions.push_back(extensionName);
    }

    VkResult UseLatestApiVersion()
    {
        if (vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkEnumerateInstanceVersion"))
            return vkEnumerateInstanceVersion(&apiVersion);
        return VK_SUCCESS;
    }

    VkResult CreateInstance(VkInstanceCreateFlags flags = 0)
    {
#ifndef NDEBUG
        std::cout << "添加扩展 ： " << "VK_LAYER_KHRONOS_validation" << "\n";
        AddInstanceLayer("VK_LAYER_KHRONOS_validation");
        std::cout << "添加扩展 ： " << VK_EXT_DEBUG_UTILS_EXTENSION_NAME << "\n";
        AddInstanceExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif
        VkApplicationInfo applicatianInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .apiVersion = apiVersion
        };
        VkInstanceCreateInfo instanceCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .flags = flags,
            .pApplicationInfo = &applicatianInfo,
            .enabledLayerCount = uint32_t(instanceLayers.size()),
            .ppEnabledLayerNames = instanceLayers.data(),
            .enabledExtensionCount = uint32_t(instanceExtensions.size()),
            .ppEnabledExtensionNames = instanceExtensions.data()
        };
        if (VkResult result = vkCreateInstance(&instanceCreateInfo, nullptr, &instance))
        {
            std::cout << std::format("[ graphicsBase ] ERROR\nFailed to create a vulkan instance!\nError code: {}\n", int32_t(result));
            return result;
        }
        //成功创建Vulkan实例后，输出Vulkan版本
        std::cout << std::format(
            "Vulkan API Version: {}.{}.{}\n",
            VK_VERSION_MAJOR(apiVersion),
            VK_VERSION_MINOR(apiVersion),
            VK_VERSION_PATCH(apiVersion));
#ifndef NDEBUG
        //创建完Vulkan实例后紧接着创建debug messenger
        CreateDebugMessenger();
#endif
        return VK_SUCCESS;
    }

    void Surface(VkSurfaceKHR surface)
    {
        if (!this->surface)
            this->surface = surface;
    }
    VkResult GetPhysicalDevices()
    {
        uint32_t deviceCount;
        if (VkResult result = vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr))
        {
            std::cout << std::format("[ graphicsBase ] ERROR\nFailed to get the count of physical devices!\nError code: {}\n", int32_t(result));
            return result;
        }
        if (!deviceCount)
            std::cout << std::format("[ graphicsBase ] ERROR\nFailed to find any physical device supports vulkan!\n"),
            abort();
        availablePhysicalDevices.resize(deviceCount);
        VkResult result = vkEnumeratePhysicalDevices(instance, &deviceCount, availablePhysicalDevices.data());
        if (result)
            std::cout << std::format("[ graphicsBase ] ERROR\nFailed to enumerate physical devices!\nError code: {}\n", int32_t(result));
        return result;
    }
    VkResult DeterminePhysicalDevice(uint32_t deviceIndex = 0, bool enableGraphicsQueue = true, bool enableComputeQueue = true)
    {
        //定义一个特殊值用于标记一个队列族索引已被找过但未找到
        static constexpr uint32_t notFound = INT32_MAX; //== VK_QUEUE_FAMILY_IGNORED & INT32_MAX
        //定义队列族索引组合的结构体
        struct queueFamilyIndexCombination {
            uint32_t graphics = VK_QUEUE_FAMILY_IGNORED;
            uint32_t presentation = VK_QUEUE_FAMILY_IGNORED;
            uint32_t compute = VK_QUEUE_FAMILY_IGNORED;
        };
        //queueFamilyIndexCombinations用于为各个物理设备保存一份队列族索引组合
        static std::vector<queueFamilyIndexCombination> queueFamilyIndexCombinations(availablePhysicalDevices.size());
        auto& [ig, ip, ic] = queueFamilyIndexCombinations[deviceIndex];

        //如果有任何队列族索引已被找过但未找到，返回VK_RESULT_MAX_ENUM
        if (ig == notFound && enableGraphicsQueue ||
            ip == notFound && surface ||
            ic == notFound && enableComputeQueue)
            return VK_RESULT_MAX_ENUM;

        //如果有任何队列族索引应被获取但还未被找过
        if (ig == VK_QUEUE_FAMILY_IGNORED && enableGraphicsQueue ||
            ip == VK_QUEUE_FAMILY_IGNORED && surface ||
            ic == VK_QUEUE_FAMILY_IGNORED && enableComputeQueue)
        {
            uint32_t indices[3];
            VkResult result = GetQueueFamilyIndices(availablePhysicalDevices[deviceIndex], enableGraphicsQueue, enableComputeQueue, indices);
            //若GetQueueFamilyIndices(...)返回VK_SUCCESS或VK_RESULT_MAX_ENUM（vkGetPhysicalDeviceSurfaceSupportKHR(...)执行成功但没找齐所需队列族），
            //说明对所需队列族索引已有结论，保存结果到queueFamilyIndexCombinations[deviceIndex]中相应变量
            //应被获取的索引若仍为VK_QUEUE_FAMILY_IGNORED，说明未找到相应队列族，VK_QUEUE_FAMILY_IGNORED（~0u）与INT32_MAX做位与得到的数值等于notFound
            if (result == VK_SUCCESS ||
                result == VK_RESULT_MAX_ENUM)
            {
                if (enableGraphicsQueue)
                    ig = indices[0] & INT32_MAX;
                if (surface)
                    ip = indices[1] & INT32_MAX;
                if (enableComputeQueue)
                    ic = indices[2] & INT32_MAX;
            }
            //如果GetQueueFamilyIndices(...)执行失败，return
            if (result)
                return result;
        }

        //若以上两个if分支皆不执行，则说明所需的队列族索引皆已被获取，从queueFamilyIndexCombinations[deviceIndex]中取得索引
        else
        {
            queueFamilyIndex_graphics = enableGraphicsQueue ? ig : VK_QUEUE_FAMILY_IGNORED;
            queueFamilyIndex_presentation = surface ? ip : VK_QUEUE_FAMILY_IGNORED;
            queueFamilyIndex_compute = enableComputeQueue ? ic : VK_QUEUE_FAMILY_IGNORED;
        }
        physicalDevice = availablePhysicalDevices[deviceIndex];
        return VK_SUCCESS;
    }
    VkResult CreateDevice(VkDeviceCreateFlags flags = 0)
    {
        float queuePriority = 1.f;
        VkDeviceQueueCreateInfo queueCreateInfos[3] = {
            {
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .queueCount = 1,
                .pQueuePriorities = &queuePriority },
            {
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .queueCount = 1,
                .pQueuePriorities = &queuePriority },
            {
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .queueCount = 1,
                .pQueuePriorities = &queuePriority } };
        uint32_t queueCreateInfoCount = 0;
        if (queueFamilyIndex_graphics != VK_QUEUE_FAMILY_IGNORED)
            queueCreateInfos[queueCreateInfoCount++].queueFamilyIndex = queueFamilyIndex_graphics;
        if (queueFamilyIndex_presentation != VK_QUEUE_FAMILY_IGNORED &&
            queueFamilyIndex_presentation != queueFamilyIndex_graphics)
            queueCreateInfos[queueCreateInfoCount++].queueFamilyIndex = queueFamilyIndex_presentation;
        if (queueFamilyIndex_compute != VK_QUEUE_FAMILY_IGNORED &&
            queueFamilyIndex_compute != queueFamilyIndex_graphics &&
            queueFamilyIndex_compute != queueFamilyIndex_presentation)
            queueCreateInfos[queueCreateInfoCount++].queueFamilyIndex = queueFamilyIndex_compute;
        VkPhysicalDeviceFeatures physicalDeviceFeatures;
        vkGetPhysicalDeviceFeatures(physicalDevice, &physicalDeviceFeatures);
        VkDeviceCreateInfo deviceCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .flags = flags,
            .queueCreateInfoCount = queueCreateInfoCount,
            .pQueueCreateInfos = queueCreateInfos,
            .enabledExtensionCount = uint32_t(deviceExtensions.size()),
            .ppEnabledExtensionNames = deviceExtensions.data(),
            .pEnabledFeatures = &physicalDeviceFeatures
        };
        if (VkResult result = vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device))
        {
            std::cout << std::format("[ graphicsBase ] ERROR\nFailed to create a vulkan logical device!\nError code: {}\n", int32_t(result));
            return result;
        }
        if (queueFamilyIndex_graphics != VK_QUEUE_FAMILY_IGNORED)
            vkGetDeviceQueue(device, queueFamilyIndex_graphics, 0, &queue_graphics);
        if (queueFamilyIndex_presentation != VK_QUEUE_FAMILY_IGNORED)
            vkGetDeviceQueue(device, queueFamilyIndex_presentation, 0, &queue_presentation);
        if (queueFamilyIndex_compute != VK_QUEUE_FAMILY_IGNORED)
            vkGetDeviceQueue(device, queueFamilyIndex_compute, 0, &queue_compute);
        vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &physicalDeviceMemoryProperties);
        //输出所用的物理设备名称
        std::cout << std::format("Renderer: {}\n", physicalDeviceProperties.deviceName);
        ExecuteCallbacks(callbacks_createDevice);
        return VK_SUCCESS;
    }

    VkResult GetSurfaceFormats()
    {
        uint32_t surfaceFormatCount;
        if (VkResult result = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, nullptr))
        {
            std::cout << std::format("[ graphicsBase ] ERROR\nFailed to get the count of surface formats!\nError code: {}\n", int32_t(result));
            return result;
        }
        if (!surfaceFormatCount)
            std::cout << std::format("[ graphicsBase ] ERROR\nFailed to find any supported surface format!\n"),
            abort();
        availableSurfaceFormats.resize(surfaceFormatCount);
        VkResult result = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, availableSurfaceFormats.data());
        if (result)
            std::cout << std::format("[ graphicsBase ] ERROR\nFailed to get surface formats!\nError code: {}\n", int32_t(result));
        return result;
    }

    VkResult SetSurfaceFormat(VkSurfaceFormatKHR surfaceFormat)
    {
        bool formatIsAvailable = false;
        if (!surfaceFormat.format)
        {
            //如果格式未指定，只匹配色彩空间，图像格式有啥就用啥
            for (auto& i : availableSurfaceFormats)
                if (i.colorSpace == surfaceFormat.colorSpace)
                {
                    swapchainCreateInfo.imageFormat = i.format;
                    swapchainCreateInfo.imageColorSpace = i.colorSpace;
                    formatIsAvailable = true;
                    break;
                }
        }
        else
        {
            //否则匹配格式和色彩空间
            for (auto& i : availableSurfaceFormats)
                if (i.format == surfaceFormat.format &&
                    i.colorSpace == surfaceFormat.colorSpace)
                {
                    swapchainCreateInfo.imageFormat = i.format;
                    swapchainCreateInfo.imageColorSpace = i.colorSpace;
                    formatIsAvailable = true;
                    break;
                }
        }
            
        //如果没有符合的格式，恰好有个语义相符的错误代码
        if (!formatIsAvailable)
            return VK_ERROR_FORMAT_NOT_SUPPORTED;
        //如果交换链已存在，调用RecreateSwapchain()重建交换链
        if (swapchain)
            return RecreateSwapchain();
        return VK_SUCCESS;
    }

    VkResult CreateSwapchain(bool limitFrameRate = true, VkSwapchainCreateFlagsKHR flags = 0)
    {
        //VkSurfaceCapabilitiesKHR相关的参数
        VkSurfaceCapabilitiesKHR surfaceCapabilities = {};
        if (VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities))
        {
            std::cout << std::format("[ graphicsBase ] ERROR\nFailed to get physical device surface capabilities!\nError code: {}\n", int32_t(result));
            return result;
        }
        //指定图像数量
        swapchainCreateInfo.minImageCount = surfaceCapabilities.minImageCount + (surfaceCapabilities.maxImageCount > surfaceCapabilities.minImageCount);
        //指定图像大小
        swapchainCreateInfo.imageExtent =
            surfaceCapabilities.currentExtent.width == -1 ?
            VkExtent2D{
                glm::clamp(defaultWindowSize.width, surfaceCapabilities.minImageExtent.width, surfaceCapabilities.maxImageExtent.width),
                glm::clamp(defaultWindowSize.height, surfaceCapabilities.minImageExtent.height, surfaceCapabilities.maxImageExtent.height) } :
                surfaceCapabilities.currentExtent;
        //swapchainCreateInfo.imageArrayLayers = 1; //跟其他不需要判断的参数一起扔后面去
        //指定变换方式
        swapchainCreateInfo.preTransform = surfaceCapabilities.currentTransform;
        //指定处理透明通道的方式
        if (surfaceCapabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR)
        {
            swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
        }
        else
        {
            for (size_t i = 0; i < 4; i++)
            {
                if (surfaceCapabilities.supportedCompositeAlpha & 1 << i)
                {
                    swapchainCreateInfo.compositeAlpha = VkCompositeAlphaFlagBitsKHR(surfaceCapabilities.supportedCompositeAlpha & 1 << i);
                    break;
                }
            }
        }  
        //指定图像用途
        swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        if (surfaceCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
            swapchainCreateInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        if (surfaceCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT)
            swapchainCreateInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        else
            std::cout << std::format("[ graphicsBase ] WARNING\nVK_IMAGE_USAGE_TRANSFER_DST_BIT isn't supported!\n");

        //指定图像格式
        if (!availableSurfaceFormats.size())
        {
            if (VkResult result = GetSurfaceFormats())
                return result;
        }
            
        if (!swapchainCreateInfo.imageFormat)
        {
            //用&&操作符来短路执行
            if (SetSurfaceFormat({ VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR }) &&
                SetSurfaceFormat({ VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR }))
            {
                //如果找不到上述图像格式和色彩空间的组合，那只能有什么用什么，采用availableSurfaceFormats中的第一组
                swapchainCreateInfo.imageFormat = availableSurfaceFormats[0].format;
                swapchainCreateInfo.imageColorSpace = availableSurfaceFormats[0].colorSpace;
                std::cout << std::format("[ graphicsBase ] WARNING\nFailed to select a four-component UNORM surface format!\n");
            }
        }
            
        //指定呈现模式
        uint32_t surfacePresentModeCount;
        if (VkResult result = vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &surfacePresentModeCount, nullptr))
        {
            std::cout << std::format("[ graphicsBase ] ERROR\nFailed to get the count of surface present modes!\nError code: {}\n", int32_t(result));
            return result;
        }
        if (!surfacePresentModeCount)
            std::cout << std::format("[ graphicsBase ] ERROR\nFailed to find any surface present mode!\n"),
            abort();
        std::vector<VkPresentModeKHR> surfacePresentModes(surfacePresentModeCount);
        if (VkResult result = vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &surfacePresentModeCount, surfacePresentModes.data()))
        {
            std::cout << std::format("[ graphicsBase ] ERROR\nFailed to get surface present modes!\nError code: {}\n", int32_t(result));
            return result;
        }
        swapchainCreateInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        if (!limitFrameRate)
            for (size_t i = 0; i < surfacePresentModeCount; i++)
                if (surfacePresentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
                {
                    swapchainCreateInfo.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
                    break;
                }

        //剩余参数
        swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapchainCreateInfo.flags = flags;
        swapchainCreateInfo.surface = surface;
        swapchainCreateInfo.imageArrayLayers = 1;
        swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapchainCreateInfo.clipped = VK_TRUE;

        //创建交换链
        if (VkResult result = CreateSwapchain_Internal())
            return result;
        //执行回调函数，ExecuteCallbacks(...)见后文
        ExecuteCallbacks(callbacks_createSwapchain);
        return VK_SUCCESS;

    }

    void InstanceLayers(const std::vector<const char*>& layerNames)
    {
        instanceLayers = layerNames;
    }
    void InstanceExtensions(const std::vector<const char*>& extensionNames)
    {
        instanceExtensions = extensionNames;
    }
    void DeviceExtensions(const std::vector<const char*>& extensionNames)
    {
        deviceExtensions = extensionNames;
    }

    VkResult RecreateSwapchain()
    {
        VkSurfaceCapabilitiesKHR surfaceCapabilities = {};
        if (VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities))
        {
            std::cout << std::format("[ graphicsBase ] ERROR\nFailed to get physical device surface capabilities!\nError code: {}\n", int32_t(result));
            return result;
        }

        if (surfaceCapabilities.currentExtent.width == 0 || surfaceCapabilities.currentExtent.height == 0)
            return VK_SUBOPTIMAL_KHR;
        swapchainCreateInfo.imageExtent = surfaceCapabilities.currentExtent;
        swapchainCreateInfo.oldSwapchain = swapchain;
        VkResult result = vkQueueWaitIdle(queue_graphics);

        if (result == VK_SUCCESS && queue_graphics != queue_presentation)
            result = vkQueueWaitIdle(queue_presentation);

        if (result)
        {
            std::cout << std::format("[ graphicsBase ] ERROR\nFailed to wait for the queue to be idle!\nError code: {}\n", int32_t(result));
            return result;
        }
        //销毁旧交换链相关对象
        ExecuteCallbacks(callbacks_destroySwapchain);
        for (auto& i : swapchainImageViews)
            if (i)
                vkDestroyImageView(device, i, nullptr);
        swapchainImageViews.resize(0);
        //创建新交换链及与之相关的对象
        if (VkResult result = CreateSwapchain_Internal())
            return result;
        //执行回调函数，ExecuteCallbacks(...)见后文
        ExecuteCallbacks(callbacks_createSwapchain);
        return VK_SUCCESS;
    }

    VkResult WaitIdle() const
    {
        VkResult result = vkDeviceWaitIdle(device);
        if (result)
            std::cout << std::format("[ graphicsBase ] ERROR\nFailed to wait for the device to be idle!\nError code: {}\n", int32_t(result));
        return result;
    }

    VkResult RecreateDevice(VkDeviceCreateFlags flags = 0)
    {
        //销毁原有的逻辑设备
        if (device)
        {
            if (VkResult result = WaitIdle();
                result != VK_SUCCESS &&
                result != VK_ERROR_DEVICE_LOST)
                return result;
            if (swapchain)
            {
                //调用销毁交换链时的回调函数
                ExecuteCallbacks(callbacks_destroySwapchain);
                //销毁交换链图像的image view
                for (auto& i : swapchainImageViews)
                    if (i)
                        vkDestroyImageView(device, i, nullptr);
                swapchainImageViews.resize(0);
                //销毁交换链
                vkDestroySwapchainKHR(device, swapchain, nullptr);
                //重置交换链handle
                swapchain = VK_NULL_HANDLE;
                //重置交换链创建信息
                swapchainCreateInfo = {};
            }
            ExecuteCallbacks(callbacks_destroyDevice);
            vkDestroyDevice(device, nullptr);
            device = VK_NULL_HANDLE;
        }
        //创建新的逻辑设备
        return CreateDevice(flags);
    }

    void AddCallback_CreateDevice(void(*function)())
    {
        callbacks_createDevice.push_back(function);
    }
    void AddCallback_DestroyDevice(void(*function)())
    {
        callbacks_destroyDevice.push_back(function);
    }
    void AddCallback_CreateSwapchain(void(*function)())
    {
        callbacks_createSwapchain.push_back(function);
    }
    void AddCallback_DestroySwapchain(void(*function)())
    {
        callbacks_destroySwapchain.push_back(function);
    }

    void Terminate()
    {
        this->~graphicsBase();
        instance = VK_NULL_HANDLE;
        physicalDevice = VK_NULL_HANDLE;
        device = VK_NULL_HANDLE;
        surface = VK_NULL_HANDLE;
        swapchain = VK_NULL_HANDLE;
        swapchainImages.resize(0);
        swapchainImageViews.resize(0);
        swapchainCreateInfo = {};
        debugMessenger = VK_NULL_HANDLE;
    }

    //Getter
    uint32_t CurrentImageIndex() const { return currentImageIndex; }
    //该函数用于获取交换链图像索引到currentImageIndex，以及在需要重建交换链时调用RecreateSwapchain()、重建交换链后销毁旧交换链
    VkResult SwapImage(VkSemaphore semaphore_imageIsAvailable)
    {
        //销毁旧交换链（若存在）
        if (swapchainCreateInfo.oldSwapchain &&
            swapchainCreateInfo.oldSwapchain != swapchain)
        {
            vkDestroySwapchainKHR(device, swapchainCreateInfo.oldSwapchain, nullptr);
            swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;
        }
        //获取交换链图像索引
        while (VkResult result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, semaphore_imageIsAvailable, VK_NULL_HANDLE, &currentImageIndex))
            switch (result)
            {
            case VK_SUBOPTIMAL_KHR:
            case VK_ERROR_OUT_OF_DATE_KHR:
                if (VkResult result = RecreateSwapchain())
                    return result;
                break; //注意重建交换链后仍需要获取图像，通过break递归，再次执行while的条件判定语句
            default:
                std::cout << std::format("[ graphicsBase ] ERROR\nFailed to acquire the next image!\nError code: {}\n", int32_t(result));
                return result;
            }
        return VK_SUCCESS;
    }

    //该函数用于将命令缓冲区提交到用于图形的队列
    VkResult SubmitCommandBuffer_Graphics(VkSubmitInfo& submitInfo, VkFence fence = VK_NULL_HANDLE) const
    {
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        VkResult result = vkQueueSubmit(queue_graphics, 1, &submitInfo, fence);
        if (result)
            std::cout << std::format("[ graphicsBase ] ERROR\nFailed to submit the command buffer!\nError code: {}\n", int32_t(result));
        return result;
    }
    //该函数用于在渲染循环中将命令缓冲区提交到图形队列的常见情形
    VkResult SubmitCommandBuffer_Graphics(VkCommandBuffer commandBuffer,
        VkSemaphore semaphore_imageIsAvailable = VK_NULL_HANDLE, VkSemaphore semaphore_renderingIsOver = VK_NULL_HANDLE, VkFence fence = VK_NULL_HANDLE,
        VkPipelineStageFlags waitDstStage_imageIsAvailable = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT) const
    {
        VkSubmitInfo submitInfo = {
            .commandBufferCount = 1,
            .pCommandBuffers = &commandBuffer
        };
        if (semaphore_imageIsAvailable)
            submitInfo.waitSemaphoreCount = 1,
            submitInfo.pWaitSemaphores = &semaphore_imageIsAvailable,
            submitInfo.pWaitDstStageMask = &waitDstStage_imageIsAvailable;
        if (semaphore_renderingIsOver)
            submitInfo.signalSemaphoreCount = 1,
            submitInfo.pSignalSemaphores = &semaphore_renderingIsOver;
        return SubmitCommandBuffer_Graphics(submitInfo, fence);
    }
    //该函数用于将命令缓冲区提交到用于图形的队列，且只使用栅栏的常见情形
    VkResult SubmitCommandBuffer_Graphics(VkCommandBuffer commandBuffer, VkFence fence = VK_NULL_HANDLE) const
    {
        VkSubmitInfo submitInfo = {
            .commandBufferCount = 1,
            .pCommandBuffers = &commandBuffer
        };
        return SubmitCommandBuffer_Graphics(submitInfo, fence);
    }
    //该函数用于将命令缓冲区提交到用于计算的队列
    VkResult SubmitCommandBuffer_Compute(VkSubmitInfo& submitInfo, VkFence fence = VK_NULL_HANDLE) const
    {
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        VkResult result = vkQueueSubmit(queue_compute, 1, &submitInfo, fence);
        if (result)
            std::cout << std::format("[ graphicsBase ] ERROR\nFailed to submit the command buffer!\nError code: {}\n", int32_t(result));
        return result;
    }
    //该函数用于将命令缓冲区提交到用于计算的队列，且只使用栅栏的常见情形
    VkResult SubmitCommandBuffer_Compute(VkCommandBuffer commandBuffer, VkFence fence = VK_NULL_HANDLE) const
    {
        VkSubmitInfo submitInfo = {
            .commandBufferCount = 1,
            .pCommandBuffers = &commandBuffer
        };
        return SubmitCommandBuffer_Compute(submitInfo, fence);
    }

    VkResult PresentImage(VkPresentInfoKHR& presentInfo)
    {
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        switch (VkResult result = vkQueuePresentKHR(queue_presentation, &presentInfo))
        {
        case VK_SUCCESS:
            return VK_SUCCESS;
        case VK_SUBOPTIMAL_KHR:
        case VK_ERROR_OUT_OF_DATE_KHR:
            return RecreateSwapchain();
        default:
            std::cout << std::format("[ graphicsBase ] ERROR\nFailed to queue the image for presentation!\nError code: {}\n", int32_t(result));
            return result;
        }
    }
    //该函数用于在渲染循环中呈现图像的常见情形
    VkResult PresentImage(VkSemaphore semaphore_renderingIsOver = VK_NULL_HANDLE)
    {
        VkPresentInfoKHR presentInfo = {
            .swapchainCount = 1,
            .pSwapchains = &swapchain,
            .pImageIndices = &currentImageIndex
        };
        if (semaphore_renderingIsOver)
            presentInfo.waitSemaphoreCount = 1,
            presentInfo.pWaitSemaphores = &semaphore_renderingIsOver;
        return PresentImage(presentInfo);
    }

    //静态函数
    static graphicsBase& Base()
    {
        return singleton;
    }

private:
    static void ExecuteCallbacks(std::vector<void(*)()> callbacks)
    {
        for (size_t size = callbacks.size(), i = 0; i < size; i++)
            callbacks[i]();
    }

        
};
inline graphicsBase graphicsBase::singleton;

#define DestroyHandleBy(Func) if (handle) { Func(graphicsBase::Base().Device(), handle, nullptr); handle = VK_NULL_HANDLE; }
#define MoveHandle handle = other.handle; other.handle = VK_NULL_HANDLE;
#define DefineMoveAssignmentOperator(type) type& operator=(type&& other) { this->~type(); MoveHandle; return *this; }
#define DefineHandleTypeOperator operator decltype(handle)() const { return handle; }
#define DefineAddressFunction const decltype(handle)* Address() const { return &handle; }

class fence {
    VkFence handle = VK_NULL_HANDLE;
public:
    //fence() = default;
    fence(VkFenceCreateInfo& createInfo)
    {
        Create(createInfo);
    }
    //默认构造器创建未置位的栅栏
    fence(VkFenceCreateFlags flags = 0)
    {
        Create(flags);
    }
    fence(fence&& other) noexcept { MoveHandle; }
    ~fence() { DestroyHandleBy(vkDestroyFence); }
    //Getter
    DefineHandleTypeOperator;
    DefineAddressFunction;
    //Const Function
    VkResult Wait() const
    {
        VkResult result = vkWaitForFences(graphicsBase::Base().Device(), 1, &handle, false, UINT64_MAX);
        if (result)
            std::cout << std::format("[ fence ] ERROR\nFailed to wait for the fence!\nError code: {}\n", int32_t(result));
        return result;
    }
    VkResult Reset() const
    {
        VkResult result = vkResetFences(graphicsBase::Base().Device(), 1, &handle);
        if (result)
            std::cout << std::format("[ fence ] ERROR\nFailed to reset the fence!\nError code: {}\n", int32_t(result));
        return result;
    }
    //因为“等待后立刻重置”的情形经常出现，定义此函数
    VkResult WaitAndReset() const
    {
        VkResult result = Wait();
        result || (result = Reset());
        return result;
    }
    VkResult Status() const
    {
        VkResult result = vkGetFenceStatus(graphicsBase::Base().Device(), handle);
        if (result < 0) //vkGetFenceStatus(...)成功时有两种结果，所以不能仅仅判断result是否非0
            std::cout << std::format("[ fence ] ERROR\nFailed to get the status of the fence!\nError code: {}\n", int32_t(result));
        return result;
    }
    //Non-const Function
    VkResult Create(VkFenceCreateInfo& createInfo)
    {
        createInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        VkResult result = vkCreateFence(graphicsBase::Base().Device(), &createInfo, nullptr, &handle);
        if (result)
            std::cout << std::format("[ fence ] ERROR\nFailed to create a fence!\nError code: {}\n", int32_t(result));
        return result;
    }
    VkResult Create(VkFenceCreateFlags flags = 0)
    {
        VkFenceCreateInfo createInfo = {
            .flags = flags
        };
        return Create(createInfo);
    }
};

class semaphore {
    VkSemaphore handle = VK_NULL_HANDLE;
public:
    //semaphore() = default;
    semaphore(VkSemaphoreCreateInfo& createInfo)
    {
        Create(createInfo);
    }
    //默认构造器创建未置位的信号量
    semaphore(/*VkSemaphoreCreateFlags flags*/)
    {
        Create();
    }
    semaphore(semaphore&& other) noexcept { MoveHandle; }
    ~semaphore() { DestroyHandleBy(vkDestroySemaphore); }
    //Getter
    DefineHandleTypeOperator;
    DefineAddressFunction;
    //Non-const Function
    VkResult Create(VkSemaphoreCreateInfo& createInfo)
    {
        createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        VkResult result = vkCreateSemaphore(graphicsBase::Base().Device(), &createInfo, nullptr, &handle);
        if (result)
            std::cout << std::format("[ semaphore ] ERROR\nFailed to create a semaphore!\nError code: {}\n", int32_t(result));
        return result;
    }
    VkResult Create(/*VkSemaphoreCreateFlags flags*/)
    {
        VkSemaphoreCreateInfo createInfo = {};
        return Create(createInfo);
    }
};

class commandBuffer {
    friend class commandPool; //封装命令池的commandPool类负责分配和释放命令缓冲区，需要让其能访问私有成员handle
    VkCommandBuffer handle = VK_NULL_HANDLE;
public:
    commandBuffer() = default;
    commandBuffer(commandBuffer&& other) noexcept { MoveHandle; }
    //因释放命令缓冲区的函数被我定义在封装命令池的commandPool类中，没析构器
    //Getter
    DefineHandleTypeOperator;
    DefineAddressFunction;
    //Const Function
    //这里没给inheritanceInfo设定默认参数，因为C++标准中规定对空指针解引用是未定义行为（尽管运行期不必发生，且至少MSVC编译器允许这种代码），而我又一定要传引用而非指针，因而形成了两个Begin(...)
    VkResult Begin(VkCommandBufferUsageFlags usageFlags, VkCommandBufferInheritanceInfo& inheritanceInfo) const
    {
        inheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
        VkCommandBufferBeginInfo beginInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = usageFlags,
            .pInheritanceInfo = &inheritanceInfo
        };
        VkResult result = vkBeginCommandBuffer(handle, &beginInfo);
        if (result)
            std::cout << std::format("[ commandBuffer ] ERROR\nFailed to begin a command buffer!\nError code: {}\n", int32_t(result));
        return result;
    }
    VkResult Begin(VkCommandBufferUsageFlags usageFlags = 0) const
    {
        VkCommandBufferBeginInfo beginInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = usageFlags,
        };
        VkResult result = vkBeginCommandBuffer(handle, &beginInfo);
        if (result)
            std::cout << std::format("[ commandBuffer ] ERROR\nFailed to begin a command buffer!\nError code: {}\n", int32_t(result));
        return result;
    }
    VkResult End() const
    {
        VkResult result = vkEndCommandBuffer(handle);
        if (result)
            std::cout << std::format("[ commandBuffer ] ERROR\nFailed to end a command buffer!\nError code: {}\n", int32_t(result));
        return result;
    }
};

template<typename T>
class arrayRef {
    T* const pArray = nullptr;
    size_t count = 0;
public:
    //从空参数构造，count为0
    arrayRef() = default;
    //从单个对象构造，count为1
    arrayRef(T& data) :pArray(&data), count(1) {}
    //从顶级数组构造
    template<size_t ElementCount>
    arrayRef(T(&data)[ElementCount]) : pArray(data), count(ElementCount) {}
    //从指针和元素个数构造
    arrayRef(T* pData, size_t elementCount) :pArray(pData), count(elementCount) {}
    //若T带const修饰，兼容从对应的无const修饰版本的arrayRef构造
    arrayRef(const arrayRef<std::remove_const_t<T>>& other) :pArray(other.Pointer()), count(other.Count()) {}
    //Getter
    T* Pointer() const { return pArray; }
    size_t Count() const { return count; }
    //Const Function
    T& operator[](size_t index) const { return pArray[index]; }
    T* begin() const { return pArray; }
    T* end() const { return pArray + count; }
    //Non-const Function
    //禁止复制/移动赋值（arrayRef旨在模拟“对数组的引用”，用处归根结底只是传参，故使其同C++引用的底层地址一样，防止初始化后被修改）
    arrayRef& operator=(const arrayRef&) = delete;
};

class commandPool {
    VkCommandPool handle = VK_NULL_HANDLE;
public:
    commandPool() = default;
    commandPool(VkCommandPoolCreateInfo& createInfo)
    {
        Create(createInfo);
    }
    commandPool(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags = 0)
    {
        Create(queueFamilyIndex, flags);
    }
    commandPool(commandPool&& other) noexcept { MoveHandle; }
    ~commandPool() { DestroyHandleBy(vkDestroyCommandPool); }
    //Getter
    DefineHandleTypeOperator;
    DefineAddressFunction;
    //Const Function
    VkResult AllocateBuffers(arrayRef<VkCommandBuffer> buffers, VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY) const
    {
        VkCommandBufferAllocateInfo allocateInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = handle,
            .level = level,
            .commandBufferCount = uint32_t(buffers.Count())
        };
        VkResult result = vkAllocateCommandBuffers(graphicsBase::Base().Device(), &allocateInfo, buffers.Pointer());
        if (result)
            std::cout << std::format("[ commandPool ] ERROR\nFailed to allocate command buffers!\nError code: {}\n", int32_t(result));
        return result;
    }
    VkResult AllocateBuffers(arrayRef<commandBuffer> buffers, VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY) const
    {
        return AllocateBuffers(
            { &buffers[0].handle, buffers.Count() },
            level);
    }
    void FreeBuffers(arrayRef<VkCommandBuffer> buffers) const
    {
        vkFreeCommandBuffers(graphicsBase::Base().Device(), handle, (uint32_t)buffers.Count(), buffers.Pointer());
        memset(buffers.Pointer(), 0, buffers.Count() * sizeof(VkCommandBuffer));
    }
    void FreeBuffers(arrayRef<commandBuffer> buffers) const
    {
        FreeBuffers({ &buffers[0].handle, buffers.Count() });
    }
    //Non-const Function
    VkResult Create(VkCommandPoolCreateInfo& createInfo)
    {
        createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        VkResult result = vkCreateCommandPool(graphicsBase::Base().Device(), &createInfo, nullptr, &handle);
        if (result)
            std::cout << std::format("[ commandPool ] ERROR\nFailed to create a command pool!\nError code: {}\n", int32_t(result));
        return result;
    }
    VkResult Create(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags = 0)
    {
        VkCommandPoolCreateInfo createInfo = {
            .flags = flags,
            .queueFamilyIndex = queueFamilyIndex
        };
        return Create(createInfo);
    }
};

class renderPass {
    VkRenderPass handle = VK_NULL_HANDLE;
public:
    renderPass() = default;
    renderPass(VkRenderPassCreateInfo& createInfo)
    {
        Create(createInfo);
    }
    renderPass(renderPass&& other) noexcept { MoveHandle; }
    ~renderPass() { DestroyHandleBy(vkDestroyRenderPass); }
    //Getter
    DefineHandleTypeOperator;
    DefineAddressFunction;
    //Const Function
    void CmdBegin(VkCommandBuffer commandBuffer, VkRenderPassBeginInfo& beginInfo, VkSubpassContents subpassContents = VK_SUBPASS_CONTENTS_INLINE) const
    {
        beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        beginInfo.renderPass = handle;
        vkCmdBeginRenderPass(commandBuffer, &beginInfo, subpassContents);
    }
    void CmdBegin(VkCommandBuffer commandBuffer, VkFramebuffer framebuffer, VkRect2D renderArea, arrayRef<const VkClearValue> clearValues = {}, VkSubpassContents subpassContents = VK_SUBPASS_CONTENTS_INLINE) const
    {
        VkRenderPassBeginInfo beginInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = handle,
            .framebuffer = framebuffer,
            .renderArea = renderArea,
            .clearValueCount = uint32_t(clearValues.Count()),
            .pClearValues = clearValues.Pointer()
        };
        vkCmdBeginRenderPass(commandBuffer, &beginInfo, subpassContents);
    }
    void CmdNext(VkCommandBuffer commandBuffer, VkSubpassContents subpassContents = VK_SUBPASS_CONTENTS_INLINE) const
    {
        vkCmdNextSubpass(commandBuffer, subpassContents);
    }
    void CmdEnd(VkCommandBuffer commandBuffer) const
    {
        vkCmdEndRenderPass(commandBuffer);
    }
    //Non-const Function
    VkResult Create(VkRenderPassCreateInfo& createInfo)
    {
        createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        VkResult result = vkCreateRenderPass(graphicsBase::Base().Device(), &createInfo, nullptr, &handle);
        if (result)
            std::cout << std::format("[ renderPass ] ERROR\nFailed to create a render pass!\nError code: {}\n", int32_t(result));
        return result;
    }
};

class framebuffer {
    VkFramebuffer handle = VK_NULL_HANDLE;
public:
    framebuffer() = default;
    framebuffer(VkFramebufferCreateInfo& createInfo)
    {
        Create(createInfo);
    }
    framebuffer(framebuffer&& other) noexcept { MoveHandle; }
    ~framebuffer() { DestroyHandleBy(vkDestroyFramebuffer); }
    //Getter
    DefineHandleTypeOperator;
    DefineAddressFunction;
    //Non-const Function
    VkResult Create(VkFramebufferCreateInfo& createInfo)
    {
        createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        VkResult result = vkCreateFramebuffer(graphicsBase::Base().Device(), &createInfo, nullptr, &handle);
        if (result)
            std::cout << std::format("[ framebuffer ] ERROR\nFailed to create a framebuffer!\nError code: {}\n", int32_t(result));
        return result;
    }
};

class pipelineLayout {
    VkPipelineLayout handle = VK_NULL_HANDLE;
public:
    pipelineLayout() = default;
    pipelineLayout(VkPipelineLayoutCreateInfo& createInfo)
    {
        Create(createInfo);
    }
    pipelineLayout(pipelineLayout&& other) noexcept { MoveHandle; }
    ~pipelineLayout() { DestroyHandleBy(vkDestroyPipelineLayout); }
    //Getter
    DefineHandleTypeOperator;
    DefineAddressFunction;
    //Non-const Function
    VkResult Create(VkPipelineLayoutCreateInfo& createInfo)
    {
        createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        VkResult result = vkCreatePipelineLayout(graphicsBase::Base().Device(), &createInfo, nullptr, &handle);
        if (result)
            std::cout << std::format("[ pipelineLayout ] ERROR\nFailed to create a pipeline layout!\nError code: {}\n", int32_t(result));
        return result;
    }
};

class pipeline {
    VkPipeline handle = VK_NULL_HANDLE;
public:
    pipeline() = default;
    pipeline(VkGraphicsPipelineCreateInfo& createInfo)
    {
        Create(createInfo);
    }
    pipeline(VkComputePipelineCreateInfo& createInfo)
    {
        Create(createInfo);
    }
    pipeline(pipeline&& other) noexcept { MoveHandle; }
    ~pipeline() { DestroyHandleBy(vkDestroyPipeline); }
    //Getter
    DefineHandleTypeOperator;
    DefineAddressFunction;
    //Non-const Function
    VkResult Create(VkGraphicsPipelineCreateInfo& createInfo)
    {
        createInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        VkResult result = vkCreateGraphicsPipelines(graphicsBase::Base().Device(), VK_NULL_HANDLE, 1, &createInfo, nullptr, &handle);
        if (result)
            std::cout << std::format("[ pipeline ] ERROR\nFailed to create a graphics pipeline!\nError code: {}\n", int32_t(result));
        return result;
    }
    VkResult Create(VkComputePipelineCreateInfo& createInfo)
    {
        createInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        VkResult result = vkCreateComputePipelines(graphicsBase::Base().Device(), VK_NULL_HANDLE, 1, &createInfo, nullptr, &handle);
        if (result)
            std::cout << std::format("[ pipeline ] ERROR\nFailed to create a compute pipeline!\nError code: {}\n", int32_t(result));
        return result;
    }
};

class shaderModule {
    VkShaderModule handle = VK_NULL_HANDLE;
public:
    shaderModule() = default;
    shaderModule(VkShaderModuleCreateInfo& createInfo)
    {
        Create(createInfo);
    }
    shaderModule(const char* filepath /*VkShaderModuleCreateFlags flags*/)
    {
        Create(filepath);
    }
    shaderModule(size_t codeSize, const uint32_t* pCode /*VkShaderModuleCreateFlags flags*/)
    {
        Create(codeSize, pCode);
    }
    shaderModule(shaderModule&& other) noexcept { MoveHandle; }
    ~shaderModule() { DestroyHandleBy(vkDestroyShaderModule); }
    //Getter
    DefineHandleTypeOperator;
    DefineAddressFunction;
    //Const Function
    VkPipelineShaderStageCreateInfo StageCreateInfo(VkShaderStageFlagBits stage, const char* entry = "main") const
    {
        return {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, //sType
            nullptr,                                             //pNext
            0,                                                   //flags
            stage,                                               //stage
            handle,                                              //module
            entry,                                               //pName
            nullptr                                              //pSpecializationInfo
        };
    }
    //Non-const Function
    VkResult Create(VkShaderModuleCreateInfo& createInfo)
    {
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        VkResult result = vkCreateShaderModule(graphicsBase::Base().Device(), &createInfo, nullptr, &handle);
        if (result)
            std::cout << std::format("[ shader ] ERROR\nFailed to create a shader module!\nError code: {}\n", int32_t(result));
        return result;
    }
    VkResult Create(const char* filepath /*VkShaderModuleCreateFlags flags*/)
    {
        std::ifstream file(filepath, std::ios::ate | std::ios::binary);
        if (!file)
        {
            std::cout << std::format("[ shader ] ERROR\nFailed to open the file: {}\n", filepath);
            return VK_RESULT_MAX_ENUM; //没有合适的错误代码，别用VK_ERROR_UNKNOWN
        }
        size_t fileSize = size_t(file.tellg());
        std::vector<uint32_t> binaries(fileSize / 4);
        file.seekg(0);
        file.read(reinterpret_cast<char*>(binaries.data()), fileSize);
        file.close();
        return Create(fileSize, binaries.data());
    }
    VkResult Create(size_t codeSize, const uint32_t* pCode /*VkShaderModuleCreateFlags flags*/)
    {
        VkShaderModuleCreateInfo createInfo = {
            .codeSize = codeSize,
            .pCode = pCode
        };
        return Create(createInfo);
    }
};

struct graphicsPipelineCreateInfoPack {
    VkGraphicsPipelineCreateInfo createInfo =
    { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
    //Vertex Input
    VkPipelineVertexInputStateCreateInfo vertexInputStateCi =
    { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    std::vector<VkVertexInputBindingDescription> vertexInputBindings;
    std::vector<VkVertexInputAttributeDescription> vertexInputAttributes;
    //Input Assembly
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCi =
    { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    //Tessellation
    VkPipelineTessellationStateCreateInfo tessellationStateCi =
    { VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO };
    //Viewport
    VkPipelineViewportStateCreateInfo viewportStateCi =
    { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
    std::vector<VkViewport> viewports;
    std::vector<VkRect2D> scissors;
    uint32_t dynamicViewportCount = 1; //动态视口/剪裁不会用到上述的vector，因此动态视口和剪裁的个数向这俩变量手动指定
    uint32_t dynamicScissorCount = 1;
    //Rasterization
    VkPipelineRasterizationStateCreateInfo rasterizationStateCi =
    { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    //Multisample
    VkPipelineMultisampleStateCreateInfo multisampleStateCi =
    { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    //Depth & Stencil
    VkPipelineDepthStencilStateCreateInfo depthStencilStateCi =
    { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
    //Color Blend
    VkPipelineColorBlendStateCreateInfo colorBlendStateCi =
    { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachmentStates;
    //Dynamic
    VkPipelineDynamicStateCreateInfo dynamicStateCi =
    { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    std::vector<VkDynamicState> dynamicStates;
    //--------------------
    graphicsPipelineCreateInfoPack()
    {
        SetCreateInfos();
        //若非派生管线，createInfo.basePipelineIndex不得为0，设置为-1
        createInfo.basePipelineIndex = -1;
    }
    //移动构造器，所有指针都要重新赋值
    graphicsPipelineCreateInfoPack(const graphicsPipelineCreateInfoPack& other) noexcept
    {
        createInfo = other.createInfo;
        SetCreateInfos();

        vertexInputStateCi = other.vertexInputStateCi;
        inputAssemblyStateCi = other.inputAssemblyStateCi;
        tessellationStateCi = other.tessellationStateCi;
        viewportStateCi = other.viewportStateCi;
        rasterizationStateCi = other.rasterizationStateCi;
        multisampleStateCi = other.multisampleStateCi;
        depthStencilStateCi = other.depthStencilStateCi;
        colorBlendStateCi = other.colorBlendStateCi;
        dynamicStateCi = other.dynamicStateCi;

        shaderStages = other.shaderStages;
        vertexInputBindings = other.vertexInputBindings;
        vertexInputAttributes = other.vertexInputAttributes;
        viewports = other.viewports;
        scissors = other.scissors;
        colorBlendAttachmentStates = other.colorBlendAttachmentStates;
        dynamicStates = other.dynamicStates;
        UpdateAllArrayAddresses();
    }
    //Getter，这里我没用const修饰符
    operator VkGraphicsPipelineCreateInfo& () { return createInfo; }
    //Non-const Function
    //该函数用于将各个vector中数据的地址赋值给各个创建信息中相应成员，并相应改变各个count
    void UpdateAllArrays()
    {
        createInfo.stageCount = (uint32_t)shaderStages.size();
        vertexInputStateCi.vertexBindingDescriptionCount = (uint32_t)vertexInputBindings.size();
        vertexInputStateCi.vertexAttributeDescriptionCount = (uint32_t)vertexInputAttributes.size();
        viewportStateCi.viewportCount = viewports.size() ? uint32_t(viewports.size()) : dynamicViewportCount;
        viewportStateCi.scissorCount = scissors.size() ? uint32_t(scissors.size()) : dynamicScissorCount;
        colorBlendStateCi.attachmentCount = (uint32_t)colorBlendAttachmentStates.size();
        dynamicStateCi.dynamicStateCount = (uint32_t)dynamicStates.size();
        UpdateAllArrayAddresses();
    }
private:
    //该函数用于将创建信息的地址赋值给basePipelineIndex中相应成员
    void SetCreateInfos()
    {
        createInfo.pVertexInputState = &vertexInputStateCi;
        createInfo.pInputAssemblyState = &inputAssemblyStateCi;
        createInfo.pTessellationState = &tessellationStateCi;
        createInfo.pViewportState = &viewportStateCi;
        createInfo.pRasterizationState = &rasterizationStateCi;
        createInfo.pMultisampleState = &multisampleStateCi;
        createInfo.pDepthStencilState = &depthStencilStateCi;
        createInfo.pColorBlendState = &colorBlendStateCi;
        createInfo.pDynamicState = &dynamicStateCi;
    }
    //该函数用于将各个vector中数据的地址赋值给各个创建信息中相应成员，但不改变各个count
    void UpdateAllArrayAddresses()
    {
        createInfo.pStages = shaderStages.data();
        vertexInputStateCi.pVertexBindingDescriptions = vertexInputBindings.data();
        vertexInputStateCi.pVertexAttributeDescriptions = vertexInputAttributes.data();
        viewportStateCi.pViewports = viewports.data();
        viewportStateCi.pScissors = scissors.data();
        colorBlendStateCi.pAttachments = colorBlendAttachmentStates.data();
        dynamicStateCi.pDynamicStates = dynamicStates.data();
    }
};