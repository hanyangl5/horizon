#include "dx12_shader_compiler.h"
#include <core/log.h>
#include <core/path.h>
#include <fstream>
#include <sstream>

#include "dx12_utils.h"

#ifdef _WIN32
#include <d3dcompiler.h>
#include <windows.h>

#pragma comment(lib, "d3dcompiler.lib")

// DXC API
#include <dxcapi.h>
#pragma comment(lib, "dxcompiler.lib")
#endif

namespace Horizon::Backend
{

#ifdef _WIN32
// DXC compiler instances (lazy initialized)
static IDxcCompiler3 *g_dxc_compiler = nullptr;
static IDxcUtils *g_dxc_utils = nullptr;
static IDxcIncludeHandler *g_dxc_include_handler = nullptr;
static HMODULE g_dxc_library = nullptr;

#endif

std::wstring DX12ShaderCompiler::GetShaderProfileDXC(ShaderType type)
{
    // For DXC - Shader Model 6.0+
    switch (type)
    {
    case ShaderType::VERTEX_SHADER:
        return L"vs_6_0";
    case ShaderType::PIXEL_SHADER:
        return L"ps_6_0";
    case ShaderType::COMPUTE_SHADER:
        return L"cs_6_0";
    default:
        LOG_ERROR("Unsupported shader type for DX12");
        return L"";
    }
}

bool DX12ShaderCompiler::InitializeDXC()
{
#ifdef _WIN32
    if (g_dxc_compiler != nullptr)
    {
        return true; // Already initialized
    }

    // Try to load dxcompiler.dll
    g_dxc_library = LoadLibraryW(L"dxcompiler.dll");
    if (g_dxc_library == nullptr)
    {
        // Try common installation paths
        const wchar_t *dxc_paths[] = {
            L"C:\\Program Files (x86)\\Windows Kits\\10\\bin\\10.0.22621.0\\x64\\dxcompiler.dll",
            L"C:\\Program Files (x86)\\Windows Kits\\10\\bin\\10.0.19041.0\\x64\\dxcompiler.dll",
            L"C:\\Windows\\System32\\dxcompiler.dll",
        };

        for (const auto &path : dxc_paths)
        {
            g_dxc_library = LoadLibraryW(path);
            if (g_dxc_library != nullptr)
            {
                break;
            }
        }
    }

    if (g_dxc_library == nullptr)
    {
        LOG_WARN("Failed to load dxcompiler.dll. Falling back to FXC (Shader Model 5.1).");
        return false;
    }

    // Get DxcCreateInstance function
    typedef HRESULT(__stdcall * DxcCreateInstanceProc)(REFCLSID, REFIID, LPVOID *);
    DxcCreateInstanceProc DxcCreateInstance = (DxcCreateInstanceProc)GetProcAddress(g_dxc_library, "DxcCreateInstance");
    if (DxcCreateInstance == nullptr)
    {
        LOG_WARN("Failed to get DxcCreateInstance. Falling back to FXC (Shader Model 5.1).");
        FreeLibrary(g_dxc_library);
        g_dxc_library = nullptr;
        return false;
    }

    // Create DXC compiler
    HRESULT hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&g_dxc_compiler));
    if (FAILED(hr))
    {
        LOG_WARN("Failed to create DXC compiler. Falling back to FXC (Shader Model 5.1).");
        FreeLibrary(g_dxc_library);
        g_dxc_library = nullptr;
        return false;
    }

    // Create DXC utils
    hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&g_dxc_utils));
    if (FAILED(hr))
    {
        LOG_WARN("Failed to create DXC utils. Falling back to FXC (Shader Model 5.1).");
        g_dxc_compiler->Release();
        g_dxc_compiler = nullptr;
        FreeLibrary(g_dxc_library);
        g_dxc_library = nullptr;
        return false;
    }

    // Create default include handler
    hr = g_dxc_utils->CreateDefaultIncludeHandler(&g_dxc_include_handler);
    if (FAILED(hr))
    {
        LOG_WARN("Failed to create DXC include handler. Falling back to FXC (Shader Model 5.1).");
        g_dxc_utils->Release();
        g_dxc_utils = nullptr;
        g_dxc_compiler->Release();
        g_dxc_compiler = nullptr;
        FreeLibrary(g_dxc_library);
        g_dxc_library = nullptr;
        return false;
    }

    LOG_DEBUG("DXC compiler initialized successfully. Using Shader Model 6.0.");
    return true;
#else
    return false;
#endif
}

void DX12ShaderCompiler::CleanupDXC()
{
#ifdef _WIN32
    if (g_dxc_include_handler)
    {
        g_dxc_include_handler->Release();
        g_dxc_include_handler = nullptr;
    }
    if (g_dxc_utils)
    {
        g_dxc_utils->Release();
        g_dxc_utils = nullptr;
    }
    if (g_dxc_compiler)
    {
        g_dxc_compiler->Release();
        g_dxc_compiler = nullptr;
    }
    if (g_dxc_library)
    {
        FreeLibrary(g_dxc_library);
        g_dxc_library = nullptr;
    }
#endif
}

std::vector<u8> DX12ShaderCompiler::CompileHLSLWithDXC(const Path &hlsl_path, ShaderType shader_type,
                                                       const char *entry_point, const Path &shader_dir)
{
#ifdef _WIN32
    if (!InitializeDXC())
    {
        return {}; // Fallback to FXC
    }

    // Read HLSL source
    std::ifstream file(hlsl_path.c_str(), std::ios::binary);
    if (!file.is_open())
    {
        LOG_ERROR("Failed to open shader file: {}", hlsl_path.c_str());
        return {};
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();
    file.close();

    // Create source blob
    IDxcBlobEncoding *source_blob = nullptr;
    HRESULT hr = g_dxc_utils->CreateBlob(source.c_str(), static_cast<UINT32>(source.length()), CP_UTF8, &source_blob);
    if (FAILED(hr))
    {
        LOG_ERROR("Failed to create source blob for DXC compilation");
        return {};
    }

    // Get shader profile
    std::wstring profile = GetShaderProfileDXC(shader_type);
    if (profile.empty())
    {
        source_blob->Release();
        return {};
    }

    // Prepare arguments (store strings to keep them alive)
    // std::vector<std::wstring> argument_strings;
    std::vector<LPCWSTR> arguments;

    // argument_strings.push_back(L"-T");
    arguments.push_back(L"-T ");

    // std::wstring profile_wide = profile;
    // argument_strings.push_back(profile_wide);
    arguments.push_back(profile.c_str());

    // argument_strings.push_back(L"-E");
    arguments.push_back(L"-E");

    // std::wstring entry_point_wide = StringToWString(std::string(entry_point));
    // argument_strings.push_back(entry_point_wide);
    std::wstring e = StringToWString(entry_point);
    arguments.push_back(e.c_str());

    // Add include directory
    // argument_strings.push_back(L"-I");
    arguments.push_back(L"-I");

    // std::wstring shader_dir_wide = StringToWString(shader_dir.string());
    // argument_strings.push_back(shader_dir_wide);
    std::wstring dir = StringToWString(shader_dir.c_str());
    arguments.push_back(dir.c_str());

    //#ifdef _DEBUG
    //    argument_strings.push_back(L"-Zi"); // Enable debug information
    //    arguments.push_back(argument_strings.back().c_str());
    //    argument_strings.push_back(L"-Od"); // Disable optimizations
    //    arguments.push_back(argument_strings.back().c_str());
    //#else
    //    argument_strings.push_back(L"-O3"); // Optimization level 3
    //    arguments.push_back(argument_strings.back().c_str());
    //#endif

    // Compile
    DxcBuffer source_buffer = {};
    source_buffer.Ptr = source_blob->GetBufferPointer();
    source_buffer.Size = source_blob->GetBufferSize();
    source_buffer.Encoding = DXC_CP_UTF8;

    // Add source file name for better error messages (must be after all other arguments)
    // std::wstring hlsl_path_wide = StringToWString(hlsl_path.string());
    // argument_strings.push_back(hlsl_path_wide);
    // arguments.push_back(argument_strings.back().c_str());

    IDxcResult *compile_result = nullptr;
    hr = g_dxc_compiler->Compile(&source_buffer, arguments.data(), static_cast<UINT32>(arguments.size()),
                                 g_dxc_include_handler, IID_PPV_ARGS(&compile_result));

    source_blob->Release();

    if (FAILED(hr))
    {
        LOG_ERROR("DXC compilation failed");
        if (compile_result)
        {
            compile_result->Release();
        }
        return {};
    }

    // Check compilation status
    HRESULT compile_status = S_OK;
    compile_result->GetStatus(&compile_status);

    if (FAILED(compile_status))
    {
        // Get error messages
        IDxcBlobUtf8 *errors = nullptr;
        compile_result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);
        if (errors && errors->GetStringLength() > 0)
        {
            LOG_ERROR("DXC shader compilation failed: {}", errors->GetStringPointer());
        }
        if (errors)
        {
            errors->Release();
        }
        compile_result->Release();
        return {};
    }

    // Get compiled shader bytecode
    IDxcBlob *shader_blob = nullptr;
    compile_result->GetResult(&shader_blob);
    if (!shader_blob)
    {
        LOG_ERROR("Failed to get compiled shader bytecode from DXC");
        compile_result->Release();
        return {};
    }

    // Copy bytecode to vector
    std::vector<u8> bytecode(static_cast<size_t>(shader_blob->GetBufferSize()));
    memcpy(bytecode.data(), shader_blob->GetBufferPointer(), shader_blob->GetBufferSize());

    shader_blob->Release();
    compile_result->Release();

    LOG_DEBUG("Shader compiled successfully with DXC (Shader Model 6.0)");
    return bytecode;
#else
    return {};
#endif
}

std::vector<u8> DX12ShaderCompiler::CompileHLSL(const Path &hlsl_path, ShaderType shader_type, const char *entry_point,
                                                const Path &shader_dir)
{
    // Try DXC first (supports Shader Model 6.0+)
    auto result = CompileHLSLWithDXC(hlsl_path, shader_type, entry_point, shader_dir);
    if (!result.empty())
    {
        return result;
    }
    LOG_ERROR("Failed to compile DX12 shader: {}", (void *)hlsl_path.c_str());
    return {};
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
