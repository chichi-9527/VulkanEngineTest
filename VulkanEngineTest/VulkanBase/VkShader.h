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

protected:
	static std::vector<char> ReadFile(const std::string& path);

private:
	VkShaderModule _shader_module;

	const VkDevice& _device;

	bool _is_vaild = false;
};
