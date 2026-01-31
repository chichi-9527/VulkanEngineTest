#include "VkShader.h"

#include <fstream>
#include <iostream>
#include <format>

VkEngineShaderModule::VkEngineShaderModule(const VkDevice& device, const std::string& path)
	: _shader_module(VK_NULL_HANDLE)
	, _device(device)
{
	auto ShaderCode = ReadFile(path);
	// assert
	if (ShaderCode.size() % sizeof(uint32_t) != 0)
	{
		std::cout << std::format("ERROR: [shader] shader code error; path : {}", path);
		return;
	}

	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = ShaderCode.size();
	createInfo.pCode = reinterpret_cast<const uint32_t*>(ShaderCode.data());

	if (VkResult result = vkCreateShaderModule(_device, &createInfo, nullptr, &_shader_module))
	{
		std::cout << std::format("ERROR : [ shader ] Failed to create a shader module! Error code: {}\n", int32_t(result));
		return;
	}

	_is_vaild = true;
}

VkEngineShaderModule::~VkEngineShaderModule()
{
	vkDestroyShaderModule(_device, _shader_module, nullptr);
}

std::vector<char> VkEngineShaderModule::ReadFile(const std::string& path)
{
	std::ifstream file(path, std::ios::ate | std::ios::binary);
	if (!file.is_open())
	{
		std::cout << std::format("ERROR : [ VulkanBase ] failed to open file : {} \n", path);
	}

	size_t fileSize = (size_t)file.tellg();
	std::vector<char> buf(fileSize);
	file.seekg(0);
	file.read(buf.data(), fileSize);
	file.close();
	return buf;
}
