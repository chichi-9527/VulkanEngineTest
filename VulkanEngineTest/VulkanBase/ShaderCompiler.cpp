#include "ShaderCompiler.h"

#include <shaderSlang/slang.h>
#include <shaderSlang/slang-com-helper.h>
#include <shaderSlang/slang-com-ptr.h>

#include <filesystem>
#include <fstream>
#include <array>

#include <iostream>

static void diagnoseIfNeeded(slang::IBlob* diagnosticsBlob)
{
    if (diagnosticsBlob != nullptr)
    {
        std::cout << (const char*)diagnosticsBlob->getBufferPointer() << std::endl;
    }
}

bool ShaderCompiler::CompilerShaders(const std::vector<std::string>& shader_paths, const std::string& out_spv_path, const std::string& out_glsl_path)
{
    // 1. Create Global Session
    Slang::ComPtr<slang::IGlobalSession> globalSession;
    SlangGlobalSessionDesc desc{};
    desc.enableGLSL = true;
    createGlobalSession(&desc, globalSession.writeRef());

    // 2. Create Session
    slang::SessionDesc sessionDesc = {};
    // 行主序
    sessionDesc.defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_ROW_MAJOR;
    // 列
    //sessionDesc.defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR;

    slang::TargetDesc targetDesc = {};
    targetDesc.format = SLANG_SPIRV;
    targetDesc.profile = globalSession->findProfile("spirv_1_5");
    // to glsl
    slang::TargetDesc targetDesc2{};
    targetDesc2.format = SLANG_GLSL;
    targetDesc2.profile = globalSession->findProfile("glsl_450");


    slang::TargetDesc targetDescs[2] = { targetDesc, targetDesc2 };
    sessionDesc.targets = targetDescs;
    sessionDesc.targetCount = 2;

    std::array<slang::CompilerOptionEntry, 1> options =
    {
        {
            slang::CompilerOptionName::EmitSpirvDirectly,
            {slang::CompilerOptionValueKind::Int, 1, 0, nullptr, nullptr}
        }
    };
    sessionDesc.compilerOptionEntries = options.data();
    sessionDesc.compilerOptionEntryCount = options.size();

    Slang::ComPtr<slang::ISession> session;
    globalSession->createSession(sessionDesc, session.writeRef());

    std::string filePath;
    std::string shaderName;
    bool hasShader = false;
    bool hasFile1 = true;
    bool hasFile2 = true;
    for (auto& path : shader_paths)
    {
        auto lp = path.find_last_of("/\\");
        if (lp + 1 < path.size())
            shaderName = path.substr(lp + 1);
        else
            shaderName = path;

        hasShader = std::filesystem::exists(path);
        if (!hasShader)
        {
            std::cout << "ERROR : [ ShaderCompiler ] not find slang file" << std::endl;
            continue;
        }
        if (!out_spv_path.empty())
        {
            hasFile1 = std::filesystem::exists(out_spv_path + ".\\" + shaderName + ".spv");
        }
        if (!out_glsl_path.empty())
        {
            hasFile1 = std::filesystem::exists(out_glsl_path + ".\\" + shaderName + ".glsl");
        }
        if (hasFile1 && hasFile2)
        {
            std::cout << std::format("INFO : [ ShaderCompiler ] file is compiled.: {}", shaderName) << std::endl;
            continue;
        }

        auto shaderSource = ReadFile(path);

        // 3. Load module
        Slang::ComPtr<slang::IModule> slangModule;
        {
            Slang::ComPtr<slang::IBlob> diagnosticsBlob;
            slangModule = session->loadModuleFromSourceString(
                "test",                  // Module name
                "test.slang",            // Module path
                shaderSource.data(),              // Shader source code
                diagnosticsBlob.writeRef()); // Optional diagnostic container
            diagnoseIfNeeded(diagnosticsBlob);
            if (!slangModule)
            {
                continue;
            }
        }
        // 4. Query Entry Points
        Slang::ComPtr<slang::IEntryPoint> entryPoint;
        {
            Slang::ComPtr<slang::IBlob> diagnosticsBlob;
            slangModule->findEntryPointByName("computeMain", entryPoint.writeRef());
            if (!entryPoint)
            {
                std::cout << "ERROR : [ ShaderCompiler ] Error getting entry point" << std::endl;
                continue;
            }
        }

        // 5. Compose Modules + Entry Points
        std::array<slang::IComponentType*, 2> componentTypes =
        {
            slangModule,
            entryPoint
        };

        Slang::ComPtr<slang::IComponentType> composedProgram;
        {
            Slang::ComPtr<slang::IBlob> diagnosticsBlob;
            SlangResult result = session->createCompositeComponentType(
                componentTypes.data(),
                componentTypes.size(),
                composedProgram.writeRef(),
                diagnosticsBlob.writeRef());
            diagnoseIfNeeded(diagnosticsBlob);
            SLANG_RETURN_ON_FAIL(result);
        }

        // 6. Link
        Slang::ComPtr<slang::IComponentType> linkedProgram;
        {
            Slang::ComPtr<slang::IBlob> diagnosticsBlob;
            SlangResult result = composedProgram->link(
                linkedProgram.writeRef(),
                diagnosticsBlob.writeRef());
            diagnoseIfNeeded(diagnosticsBlob);
            SLANG_RETURN_ON_FAIL(result);
        }

        // 7. Get Target Kernel Code
        Slang::ComPtr<slang::IBlob> spirvCode;
        {
            Slang::ComPtr<slang::IBlob> diagnosticsBlob;
            SlangResult result = linkedProgram->getEntryPointCode(
                0,
                0,
                spirvCode.writeRef(),
                diagnosticsBlob.writeRef());
            diagnoseIfNeeded(diagnosticsBlob);
            SLANG_RETURN_ON_FAIL(result);
        }

        Slang::ComPtr<slang::IBlob> glslCode;
        {
            Slang::ComPtr<slang::IBlob> diagnosticsBlob;
            SlangResult result = linkedProgram->getEntryPointCode(
                0,
                1,
                glslCode.writeRef(),
                diagnosticsBlob.writeRef());
            diagnoseIfNeeded(diagnosticsBlob);
            SLANG_RETURN_ON_FAIL(result);
        }


        std::cout << "INFO : [ ShaderCompiler ] Compiled SPIR-V done." << std::endl;

        if (glslCode)
        {
            std::cout << "INFO : [ ShaderCompiler ] Compiled GLSL done." << std::endl;
        }
        if (!out_spv_path.empty())
            WriteFile((const char*)spirvCode->getBufferPointer(), spirvCode->getBufferSize(), out_spv_path, shaderName + ".spv");
        if (!out_glsl_path.empty())
            WriteFile((const char*)glslCode->getBufferPointer(), glslCode->getBufferSize(), out_glsl_path, shaderName + ".glsl");
    }
	return true;
}

std::vector<char> ShaderCompiler::ReadFile(const std::string& path)
{
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open())
    {
        std::cout << std::format("ERROR : [ ShaderCompiler ] failed to open file : {} \n", path);
    }

    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buf(fileSize);
    file.seekg(0);
    file.read(buf.data(), fileSize);
    file.close();
    return buf;
}

bool ShaderCompiler::WriteFile(const char* data, size_t buffer_size, const std::string& directoryPath, const std::string& name)
{
    auto path = directoryPath + "\\" + name;
    std::filesystem::create_directories(directoryPath);

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file.is_open())
    {
        std::cout << "ERROR : [ ShaderCompiler ] file open error;\n";
        return false;
    }

    file.write(data, buffer_size);

    if (!file)
    {
        std::cout << "ERROR : [ ShaderCompiler ] file write error;\n";
        file.close();
        return false;
    }

    std::cout << "INFO : [ ShaderCompiler ] write done.\n";
    file.close();
    return true;
}


