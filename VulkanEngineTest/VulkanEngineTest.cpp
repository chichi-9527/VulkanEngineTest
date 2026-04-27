// VulkanEngineTest.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//
// 模拟预处理头
#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#define NOMINMAX
#endif
#include <vulkan/vulkan.h>

#include <iostream>
#include <format>
#include <sstream>

#include <filesystem>
#include <cstdlib>
#include <array>

#include "VulkanBase/VulkanBase.h"
#include "VulkanBase/Vertex.h"

#include <glm/glm.hpp>

#include <stb_image.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif // _WIN32

#include "VulkanBase/ShaderCompiler.h"

GLFWwindow* glfw_window;
GLFWmonitor* glfw_monitor;
constexpr const char* title = "VKTest";

bool InitializeWindow(VkExtent2D size, bool fullScreen = false, bool isResizable = true, bool limitFrameRate = true)
{
    if (!glfwInit())
    {
        std::cout << std::format("[ InitializeWindow ] ERROR\nFailed to initialize GLFW!\n");
        return false;
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    // 是否可拉伸
    glfwWindowHint(GLFW_RESIZABLE, isResizable);

    glfw_monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* pMode = glfwGetVideoMode(glfw_monitor);
    glfw_window = fullScreen ?
        glfwCreateWindow(pMode->width, pMode->height, title, glfw_monitor, nullptr) :
        glfwCreateWindow(size.width, size.height, title, nullptr, nullptr);

    if (!glfw_window)
    {
        std::cout << std::format("[ InitializeWindow ]\nFailed to create a glfw window!\n");
        glfwTerminate();
        return false;
    }

    glfwSetFramebufferSizeCallback(glfw_window, [](GLFWwindow* window, int width, int height) {
        
        VulkanBase::Base().FrameBufferResize(width, height);
        });

    // 用glfwGetRequiredInstanceExtensions(...)获取平台所需的扩展，若执行成功，返回一个指针，指向一个由所需扩展的名称为元素的数组，
    // 失败则返回nullptr，并意味着此设备不支持Vulkan。
    uint32_t extensionCount = 0;
    const char** extensionNames;
    extensionNames = glfwGetRequiredInstanceExtensions(&extensionCount);
    if (!extensionNames)
    {
        std::cout << std::format("[ InitializeWindow ]\nVulkan is not available on this machine!\n");
        glfwTerminate();
        return false;
    }
    for (size_t i = 0; i < extensionCount; i++)
    {
    
        VulkanBase::Base().AddInstanceExtension(extensionNames[i]);
    }

    VulkanBase::Base().AddDeviceExtension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

    //在创建window surface前创建Vulkan实例
    if (!VulkanBase::Base().InitVulkanInstance())
        return false;
        
    //创建window surface
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (VkResult result = glfwCreateWindowSurface(VulkanBase::Base().GetVkInstance(), glfw_window, nullptr, &surface))
    {
        std::cout << std::format("[ InitializeWindow ] ERROR\nFailed to create a window surface!\nError code: {}\n", int32_t(result));
        glfwTerminate();
        return false;
    }
    VulkanBase::Base().SetSurface(surface);
    VulkanBase::Base().InitVulkan();

    return true;
}

void TerminateWindow()
{
    //graphicsBase::Base().WaitIdle();
    glfwTerminate();
}

void TitleFps()
{
    static double time0 = glfwGetTime();
    static double time1;
    static double dt;
    static int dframe = -1;
    static std::stringstream info;
    time1 = glfwGetTime();
    dframe++;
    if ((dt = time1 - time0) >= 1)
    {
        info.precision(1);
        info << title << "    " << std::fixed << dframe / dt << " FPS";
        glfwSetWindowTitle(glfw_window, info.str().c_str());
        info.str(""); //别忘了在设置完窗口标题后清空所用的stringstream
        time0 = time1;
        dframe = 0;
    }
}

//#ifdef _WIN32
//void executeAndPrint(const char* command)
//{
//    FILE* pipe = _popen(command, "r");
//    if (!pipe)
//    {
//        std::cerr << "popen failed!" << std::endl;
//        return;
//    }
//
//    char buffer[128];
//    while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
//    {
//        std::cout << "lv se: " << buffer;
//    }
//
//    _pclose(pipe);
//}
//#endif // _WIN32



int main()
{
//    std::string path;
//    try
//    {
//        path = std::filesystem::current_path().string();
//    }
//    catch (const std::exception& e)
//    {
//        std::cerr << e.what() << "\n";
//        return -1;
//    }
//    std::cout << path << "\n";
//#ifdef _WIN32
//    //executeAndPrint((path + "\\shader\\vulkan\\compile.bat").c_str());
//    std::cout << "................................................\n";
//#endif // _WIN32

    //////////////////////////
    if (1)
    {
        std::vector<std::string> shaderPaths = {
        ".\\shader\\vulkan\\Slang\\fristTriangle.slang"
        };

        ShaderCompiler::CompilerShaders(shaderPaths, ".\\shader\\vulkan\\SPV", ".\\shader\\vulkan\\GLSL");
    }
    //////////////////////////
    
    //

    // init
    if (!InitializeWindow({ 1280, 720 }))
        return -1; 

	uint32_t frameIndex = 0;

    VkClearValue clearColor = { .color = { 1.f, 0.f, 0.f, 1.f } };

    while (!glfwWindowShouldClose(glfw_window))
    {
        while (glfwGetWindowAttrib(glfw_window, GLFW_ICONIFIED))
            glfwWaitEvents();

		// draw frame
		VulkanBase::Base().WaitForFence(frameIndex);
        if (int res = VulkanBase::Base().AcquireNextImage(frameIndex))
        {
            if (res == -1)
            {
                continue;
            }
            else if (res == -2)
            {
                break;
            }
        }
		VulkanBase::Base().ResetCommandBuffer();
		VulkanBase::Base().RecordCommandBuffer(frameIndex);
        if (!VulkanBase::Base().SubmitCommandBuffer(frameIndex))
        {
			std::cout << std::format("Failed to submit draw command buffer! \n");
			break;
        }
		VulkanBase::Base().Present(frameIndex);
		//VulkanBase::Base().DrawFrame();

        glfwPollEvents();
        TitleFps();

    }
	VulkanBase::Base().WaitIdle();
    VulkanBase::Base().CleanUp();
    TerminateWindow();
    return 0;
}

