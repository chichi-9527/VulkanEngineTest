#include "VulkanBase/ShaderCompiler.h"

int shaderSlangTest()
{
    std::vector<std::string> shaderPaths = {
        ".\\shader\\Slang\\test.slang"
    };

    ShaderCompiler::CompilerShaders(shaderPaths, ".\\shader\\SPV", ".\\shader\\GLSL");

    getchar();
    return 0;
}