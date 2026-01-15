// VulkanEngineTest.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include <format>
#include <sstream>


//#include "VKBase.h"
//#include "EasyVulkan.h"

#include "VulkanBase/VulkanBase.h"

#include <stb_image.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

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

    ////通过用||操作符短路执行来省去几行
    //if (//获取物理设备，并使用列表中的第一个物理设备，这里不考虑以下任意函数失败后更换物理设备的情况
    //    graphicsBase::Base().GetPhysicalDevices() ||
    //    //一个true一个false，暂时不需要计算用的队列
    //    graphicsBase::Base().DeterminePhysicalDevice(0, true, false) ||
    //    //创建逻辑设备
    //    graphicsBase::Base().CreateDevice()) return false;

    //if (graphicsBase::Base().CreateSwapchain(limitFrameRate))
    //    return false;

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

//pipelineLayout pipelineLayout_triangle; //管线布局
//pipeline pipeline_triangle;             //管线

//该函数调用easyVulkan::CreateRpwf_Screen()并存储返回的引用到静态变量
//const auto& RenderPassAndFramebuffers()
//{
//    static const auto& rpwf = CreateRpwf_Screen();
//    return rpwf;
//}
//该函数用于创建管线布局
//void CreateLayout()
//{
//    /*待后续填充*/
//}
//该函数用于创建管线
//void CreatePipeline()
//{
//    static shaderModule vert("shader/FirstTriangle.vert.spv");
//    static shaderModule frag("shader/FirstTriangle.frag.spv");
//    static VkPipelineShaderStageCreateInfo shaderStageCreateInfos_triangle[2] = {
//        vert.StageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT),
//        frag.StageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT)
//    };
//    auto Create = [] {
//        graphicsPipelineCreateInfoPack pipelineCiPack;
//        pipelineCiPack.createInfo.layout = pipelineLayout_triangle;
//        pipelineCiPack.createInfo.renderPass = RenderPassAndFramebuffers().renderPass;
//        pipelineCiPack.inputAssemblyStateCi.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
//        pipelineCiPack.viewports.emplace_back(0.f, 0.f, float(windowSize.width), float(windowSize.height), 0.f, 1.f);
//        pipelineCiPack.scissors.emplace_back(VkOffset2D{}, windowSize);
//        pipelineCiPack.multisampleStateCi.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
//        pipelineCiPack.colorBlendAttachmentStates.push_back({ .colorWriteMask = 0b1111 });
//        pipelineCiPack.UpdateAllArrays();
//        pipelineCiPack.createInfo.stageCount = 2;
//        pipelineCiPack.createInfo.pStages = shaderStageCreateInfos_triangle;
//        pipeline_triangle.Create(pipelineCiPack);
//        };
//    auto Destroy = [] {
//        pipeline_triangle.~pipeline();
//        };
//    graphicsBase::Base().AddCallback_CreateSwapchain(Create);
//    graphicsBase::Base().AddCallback_DestroySwapchain(Destroy);
//    Create();
//}

int main()
{
    if (!InitializeWindow({ 1280, 720 }))
        return -1; 

    //const auto& [renderPass, framebuffers] = RenderPassAndFramebuffers();
    //CreateLayout();
    //CreatePipeline();

    //fence fence; //以非置位状态创建栅栏
    //semaphore semaphore_imageIsAvailable;
    //semaphore semaphore_renderingIsOver;

    //commandBuffer commandBuffer;
    //commandPool commandPool(graphicsBase::Base().QueueFamilyIndex_Graphics(), VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    //commandPool.AllocateBuffers(commandBuffer);

    VkClearValue clearColor = { .color = { 1.f, 0.f, 0.f, 1.f } };

    while (!glfwWindowShouldClose(glfw_window))
    {
        while (glfwGetWindowAttrib(glfw_window, GLFW_ICONIFIED))
            glfwWaitEvents();

        
        ////获取交换链图像索引
        //graphicsBase::Base().SwapImage(semaphore_imageIsAvailable);
        ////因为帧缓冲与所获取的交换链图像一一对应，获取交换链图像索引
        //auto i = graphicsBase::Base().CurrentImageIndex();

        //commandBuffer.Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        ///*开始渲染通道*/ 
        //renderPass.CmdBegin(commandBuffer, framebuffers[i], { {}, windowSize }, clearColor);

        //vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_triangle);
        //vkCmdDraw(commandBuffer, 3, 1, 0, 0);

        ///*结束渲染通道*/ 
        //renderPass.CmdEnd(commandBuffer);
        //commandBuffer.End();

        ///*提交命令缓冲区*/
        //graphicsBase::Base().SubmitCommandBuffer_Graphics(commandBuffer, semaphore_imageIsAvailable, semaphore_renderingIsOver, fence);
        ///*呈现图像*/
        //graphicsBase::Base().PresentImage(semaphore_renderingIsOver);

        glfwPollEvents();
        TitleFps();

        //等待并重置fence
        //fence.WaitAndReset();
    }
    VulkanBase::Base().CleanUp();
    TerminateWindow();
    return 0;
}

