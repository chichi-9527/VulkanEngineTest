#include "VulkanBase/ShaderCompiler.h"

int shaderSlangTest()
{
    std::vector<std::pair<std::string, ShaderCompiler::CompFileFindCriteria>> shaderPaths = {
        {".\\shader\\Slang\\test.slang",ShaderCompiler::CompFileFindCriteria::STAGE_DEFAULT}
    };

    ShaderCompiler::CompileShaders(shaderPaths, ".\\shader\\SPV", ".\\shader\\GLSL");

    getchar();
    return 0;
}