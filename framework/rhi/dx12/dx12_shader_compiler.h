#pragma once

#include <core/path.h>
#include <string>
#include <vector>

#include <rhi/enums.h>


#include "dxcapi.h"

namespace Horizon::Backend
{

class DX12ShaderCompiler
{
  public:
    // Compile HLSL to DXBC/DXIL using DXC or FXC
    // Returns compiled shader bytecode
    static IDxcBlob* CompileHLSL(const Path &hlsl_path, ShaderType shader_type, const char *entry_point,
                                       const Path &shader_dir);

    // Check if shader needs recompilation
    static bool NeedsRecompilation(const Path &hlsl_path, const Path &cached_path);

  private:
    // static std::string GetShaderProfile(ShaderType type);
    static std::wstring GetShaderProfileDXC(ShaderType type);

    // Compile using DXC API (supports Shader Model 6.0+)
    static IDxcBlob *CompileHLSLWithDXC(const Path &hlsl_path, ShaderType shader_type, const char *entry_point,
                                              const Path &shader_dir);

    static std::string FindDXCExecutable();
    static std::vector<std::string> GetIncludeDirectories(const Path &shader_dir);

    // Initialize DXC compiler (lazy initialization)
    static bool InitializeDXC();
    static void CleanupDXC();

  public:
    // Get DXC reflection interface for DXIL shader reflection
    static void *GetDXCReflection();
    
    // Get DXC utils for creating blobs
    static void *GetDXCUtils();
};

} // namespace Horizon::Backend
