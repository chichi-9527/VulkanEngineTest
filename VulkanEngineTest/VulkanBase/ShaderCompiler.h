#pragma once

#include <string>
#include <vector>

class ShaderCompiler
{
public:
	/*SLANG_STAGE_VERTEX,
	SLANG_STAGE_HULL,
	SLANG_STAGE_DOMAIN,
	SLANG_STAGE_GEOMETRY,
	SLANG_STAGE_FRAGMENT,
	SLANG_STAGE_COMPUTE,
	SLANG_STAGE_RAY_GENERATION,
	SLANG_STAGE_INTERSECTION,
	SLANG_STAGE_ANY_HIT,
	SLANG_STAGE_CLOSEST_HIT,
	SLANG_STAGE_MISS,
	SLANG_STAGE_CALLABLE,
	SLANG_STAGE_MESH,
	SLANG_STAGE_AMPLIFICATION,
	SLANG_STAGE_DISPATCH,*/
	// 根据此判断查找哪些文件（如顶点<vert>/片段<frag>着色器等）
	enum CompFileFindCriteria : uint64_t
	{
		STAGE_NONE =					0x1,
		STAGE_VERTEX =					0x2,
		STAGE_HULL =					0x4,
		STAGE_DOMAIN =					0x8,
		STAGE_GEOMETRY =				0x10,
		STAGE_FRAGMENT =				0x20,
		STAGE_COMPUTE =					0x40,
		STAGE_RAY_GENERATION =			0x80,
		STAGE_INTERSECTION =			0x100,
		STAGE_ANY_HIT =					0x200,
		STAGE_CLOSEST_HIT =				0x400,
		STAGE_MISS =					0x800,
		STAGE_CALLABLE =				0x1000,
		STAGE_MESH =					0x2000,
		STAGE_AMPLIFICATION =			0x4000,
		STAGE_DISPATCH =				0x8000,


		// define
		STAGE_DEFAULT = STAGE_VERTEX | STAGE_FRAGMENT
	};

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

