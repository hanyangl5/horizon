#include "dx12_shader_compiler.h"
#include <core/log.h>
#include <core/path.h>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#include <d3dcompiler.h>

#pragma comment(lib, "d3dcompiler.lib")
#endif

namespace Horizon::Backend
{

std::string DX12ShaderCompiler::GetShaderProfile(ShaderType type)
{
    switch (type)
    {
    case ShaderType::VERTEX_SHADER:
        return "vs_6_0";
    case ShaderType::PIXEL_SHADER:
        return "ps_6_0";
    case ShaderType::COMPUTE_SHADER:
        return "cs_6_0";
    default:
        LOG_ERROR("Unsupported shader type for DX12");
        return "";
    }
}

std::vector<u8> DX12ShaderCompiler::CompileHLSL(const Path &hlsl_path, ShaderType shader_type, const char *entry_point,
                                                 const Path &shader_dir)
{
#ifdef _WIN32
    std::string profile = GetShaderProfile(shader_type);
    if (profile.empty())
    {
        return {};
    }

    // Read HLSL source
    std::ifstream file(hlsl_path.c_str());
    if (!file.is_open())
    {
        LOG_ERROR("Failed to open shader file: {}", hlsl_path.c_str());
        return {};
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();
    file.close();

    // Compile shader using D3DCompile
    ID3DBlob *shader_blob = nullptr;
    ID3DBlob *error_blob = nullptr;

    std::string shader_dir_str = shader_dir.string();
    std::vector<D3D_SHADER_MACRO> defines;
    defines.push_back({nullptr, nullptr});

    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

    HRESULT hr = D3DCompile(source.c_str(), source.length(), hlsl_path.c_str(), defines.data(), D3D_COMPILE_STANDARD_FILE_INCLUDE,
                            entry_point, profile.c_str(), flags, 0, &shader_blob, &error_blob);

    if (FAILED(hr))
    {
        if (error_blob)
        {
            LOG_ERROR("Shader compilation failed: {}", (char *)error_blob->GetBufferPointer());
            error_blob->Release();
        }
        return {};
    }

    if (error_blob)
    {
        error_blob->Release();
    }

    // Copy bytecode to vector
    std::vector<u8> bytecode(shader_blob->GetBufferSize());
    memcpy(bytecode.data(), shader_blob->GetBufferPointer(), shader_blob->GetBufferSize());

    shader_blob->Release();
    return bytecode;
#else
    LOG_ERROR("DX12 shader compilation is only supported on Windows");
    return {};
#endif
}

bool DX12ShaderCompiler::NeedsRecompilation(const Path &hlsl_path, const Path &cached_path)
{
    if (!cached_path.exists())
    {
        return true;
    }

    auto hlsl_time = hlsl_path.last_write_time();
    auto cached_time = cached_path.last_write_time();

    return hlsl_time > cached_time;
}

std::string DX12ShaderCompiler::FindDXCExecutable()
{
    // TODO: Implement DXC executable finding
    // For now, we use D3DCompile from d3dcompiler.lib
    return "";
}

std::vector<std::string> DX12ShaderCompiler::GetIncludeDirectories(const Path &shader_dir)
{
    std::vector<std::string> includes;
    includes.push_back(shader_dir.string());
    return includes;
}

} // namespace Horizon::Backend
