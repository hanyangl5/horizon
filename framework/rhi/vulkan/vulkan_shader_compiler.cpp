#include "vulkan_shader_compiler.h"

#include <core/log.h>
#include <core/path.h>
#include <rhi/enums.h>

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

#ifdef _WIN32
#include <process.h>
#include <windows.h>
#else
#include <cstdio>
#include <unistd.h>
#endif

namespace Horizon::Backend
{

bool ShaderCompiler::NeedsRecompilation(const Path &hlsl_path, const Path &spv_path)
{
    // If SPV doesn't exist, need to compile
    if (!spv_path.exists())
    {
        return true;
    }

    // If HLSL doesn't exist, can't compile
    if (!hlsl_path.exists())
    {
        LOG_ERROR("HLSL source file not found: {}", hlsl_path.string());
        return false;
    }

    // Check modification times
    auto hlsl_time = hlsl_path.last_write_time();
    auto spv_time = spv_path.last_write_time();

    // If HLSL is newer than SPV, need to recompile
    return hlsl_time > spv_time;
}

std::string ShaderCompiler::GetShaderProfile(ShaderType type)
{
    switch (type)
    {
    case ShaderType::VERTEX_SHADER:
        return "vs_6_0";
    case ShaderType::PIXEL_SHADER:
        return "ps_6_0";
    case ShaderType::COMPUTE_SHADER:
        return "cs_6_0";
    // case ShaderType::GEOMETRY_SHADER:
    //    return "gs_6_0";
    default:
        LOG_ERROR("Unsupported shader type for HLSL compilation");
        return "vs_6_0";
    }
}

// std::string ShaderCompiler::FindDXCExecutable() {
//
//    // Check environment variables
//    char *dxc_path_env = nullptr;
//    size_t size = 0;
//    if (_dupenv_s(&dxc_path_env, &size, "DXC_PATH") == 0 && dxc_path_env != nullptr) {
//        fs::path dxc_path = fs::path(dxc_path_env);
//        free(dxc_path_env);
//        if (fs::is_directory(dxc_path)) {
//#ifdef _WIN32
//            dxc_path /= "dxc.exe";
//#else
//            dxc_path /= "dxc";
//#endif
//        }
//        if (fs::exists(dxc_path)) {
//            return dxc_path.string();
//        }
//    }
//    if (dxc_path_env) free(dxc_path_env);
//
//    char *hsl_compiler_dxc = nullptr;
//    if (_dupenv_s(&hsl_compiler_dxc, &size, "HSL_COMPILER_DXC") == 0 && hsl_compiler_dxc != nullptr) {
//        fs::path dxc_path = fs::path(hsl_compiler_dxc);
//        free(hsl_compiler_dxc);
//        if (fs::is_directory(dxc_path)) {
//#ifdef _WIN32
//            dxc_path /= "dxc.exe";
//#else
//            dxc_path /= "dxc";
//#endif
//        }
//        if (fs::exists(dxc_path)) {
//            return dxc_path.string();
//        }
//    }
//    if (hsl_compiler_dxc) free(hsl_compiler_dxc);
//
//    // Check Vulkan SDK
//    char *vulkan_sdk = nullptr;
//    if (_dupenv_s(&vulkan_sdk, &size, "VULKAN_SDK") == 0 && vulkan_sdk != nullptr) {
//        fs::path dxc_path = fs::path(vulkan_sdk) / "Bin";
//        free(vulkan_sdk);
//#ifdef _WIN32
//        dxc_path /= "dxc.exe";
//#else
//        dxc_path /= "dxc";
//#endif
//        if (fs::exists(dxc_path)) {
//            return dxc_path.string();
//        }
//    }
//    if (vulkan_sdk) free(vulkan_sdk);
//
//    // Try to find in PATH using which/where
//#ifdef _WIN32
//    // On Windows, try where command
//    FILE *pipe = _popen("where dxc.exe", "r");
//    if (pipe) {
//        char buffer[512];
//        if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
//            std::string result = buffer;
//            result.erase(result.find_last_not_of(" \n\r\t") + 1);
//            _pclose(pipe);
//            if (fs::exists(result)) {
//                return result;
//            }
//        }
//        _pclose(pipe);
//    }
//#else
//    // On Unix-like systems, try which
//    FILE *pipe = popen("which dxc", "r");
//    if (pipe) {
//        char buffer[512];
//        if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
//            std::string result = buffer;
//            result.erase(result.find_last_not_of(" \n\r\t") + 1);
//            pclose(pipe);
//            if (fs::exists(result)) {
//                return result;
//            }
//        }
//        pclose(pipe);
//    }
//#endif
//
//    LOG_ERROR("DXC executable not found. Please set DXC_PATH or VULKAN_SDK environment variable.");
//    return "";
//}

std::vector<std::string> ShaderCompiler::GetIncludeDirectories(const Path &shader_dir)
{
    std::vector<std::string> includes;

    // Add shader_dir/include if it exists
    Path include_dir = shader_dir / "include";
    if (include_dir.exists() && include_dir.is_directory())
    {
        includes.push_back(include_dir.string());
    }

    // Add shader_dir itself
    includes.push_back(shader_dir.string());

    return includes;
}

bool ShaderCompiler::CompileHLSLToSPIRV(const Path &hlsl_path, const Path &output_spv_path, ShaderType shader_type,
                                        const Path &shader_dir, const Path &saved_shader_dir)
{
    // Check if source exists
    if (!hlsl_path.exists())
    {
        LOG_ERROR("HLSL source file not found: {}", hlsl_path.string());
        return false;
    }

    // Find DXC executable
    // std::string dxc_exe = FindDXCExecutable();
    // if (dxc_exe.empty()) {
    //    return false;
    //}

    // Create output directory if it doesn't exist
    saved_shader_dir.create_directories();

    // Get shader profile
    std::string profile = GetShaderProfile(shader_type);

    // Get include directories
    auto include_dirs = GetIncludeDirectories(shader_dir);

    // Build DXC command
    std::ostringstream cmd;
    cmd << "dxc -spirv";
    cmd << " -T " << profile;
    cmd << " -E main";
    cmd << " -Fo \"" << output_spv_path.string() << "\"";
    // Preserve reflection metadata (OpName, OpDecorate, etc.) for spirv-reflect
    cmd << " -fspv-reflect";

    // Add include directories
    for (const auto &inc_dir : include_dirs)
    {
        cmd << " -I \"" << inc_dir << "\"";
    }

    cmd << " \"" << hlsl_path.string() << "\"";

    LOG_DEBUG("Compiling shader: {}", cmd.str());

    // Execute DXC using system() for simplicity
    // Note: For production, consider using DXC API directly for better error handling
    int result = system(cmd.str().c_str());
    if (result != 0)
    {
        LOG_ERROR("DXC compilation failed with exit code: {}", result);
        return false;
    }

    // Verify output file was created
    if (!output_spv_path.exists())
    {
        LOG_ERROR("DXC compilation succeeded but output file not found: {}", output_spv_path.string());
        return false;
    }

    LOG_DEBUG("Shader compiled successfully: {}", output_spv_path.string());
    return true;
}

} // namespace Horizon::Backend
