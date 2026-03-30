#pragma once

#include <string>
#include <vector>

class ShaderCompiler
{
public:
	/// <summary>
	/// 
	/// </summary>
	/// <param name="shader_paths">文件路径</param>
	/// <param name="out_spv_path">文件夹路径</param>
	/// <param name="out_glsl_path">文件夹路径</param>
	/// <returns></returns>
	static bool CompilerShaders(const std::vector<std::string>& shader_paths
		, const std::string& out_spv_path = ""
		, const std::string& out_glsl_path = "");

	static std::vector<char> ReadFile(const std::string& path);

	static bool WriteFile(const char* data, size_t buffer_size, const std::string& path, const std::string& name);



};

