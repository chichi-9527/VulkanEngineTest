#pragma once

#include <vulkan/vulkan.h>

#include <string>
#include <vector>

class VkEngineShaderModule
{
public:
	VkEngineShaderModule(const VkDevice& device, const std::string& path);
	virtual ~VkEngineShaderModule();

	bool IsVaild() const { return _is_vaild; }

	VkShaderModule GetShaderModule() const { return _shader_module; }

protected:
	static std::vector<char> ReadFile(const std::string& path);

private:
	VkShaderModule _shader_module;

	const VkDevice& _device;

	bool _is_vaild = false;
};

class VkEngineShaderStages
{
public:
	VkEngineShaderStages(const VkEngineShaderModule& vert_shader_module, const VkEngineShaderModule& frag_shader_module);
	virtual ~VkEngineShaderStages();

private:
	VkPipelineShaderStageCreateInfo _vert_shader_stage;
};