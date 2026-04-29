// 模拟预处理头
#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#define NOMINMAX
#endif
#include <vulkan/vulkan.h>

#include "Buffer.h"

#include <iostream>
#include <format>

