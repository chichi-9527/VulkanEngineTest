#pragma once
// 模拟预处理头
#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#define NOMINMAX
#endif
#include <vulkan/vulkan.h>

#include <array>
#include <glm/glm.hpp>

struct Vertex
{
    glm::vec2 position;
    glm::vec3 color;

    //  VK_VERTEX_INPUT_RATE_VERTEX：    在每个顶点之后移动到下一个数据条目
    //  VK_VERTEX_INPUT_RATE_INSTANCE    每次实例完成后，移至下一个数据条目
    static VkVertexInputBindingDescription getBindingDescription()
    {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return bindingDescription;
    }

    /*float：VK_FORMAT_R32_SFLOAT
    vec2：VK_FORMAT_R32G32_SFLOAT
    vec3：VK_FORMAT_R32G32B32_SFLOAT
    vec4：VK_FORMAT_R32G32B32A32_SFLOAT
    ivec2：VK_FORMAT_R32G32_SINT，一个包含 32 位有符号整数的 2 分量向量
    uvec4：VK_FORMAT_R32G32B32A32_UINT，一个包含 4 个分量的 32 位无符号整数的向量
    double：VK_FORMAT_R64_SFLOAT，双精度（64 位）浮点数*/
    static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions()
    {
        std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, position);

        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, color);

        return attributeDescriptions;
    }
};
