#pragma once

#include <core/path.h>
#include <string>
#include <vector>

#include <rhi/enums.h>

namespace Horizon::Backend
{

class DX12ShaderCompiler
{
  public:
    // Compile HLSL to DXBC/DXIL using DXC or FXC
    // Returns compiled shader bytecode
    static std::vector<u8> CompileHLSL(const Path &hlsl_path, ShaderType shader_type, const char *entry_point,
                                       const Path &shader_dir);

    // Check if shader needs recompilation
    static bool NeedsRecompilation(const Path &hlsl_path, const Path &cached_path);

  private:
    static std::string GetShaderProfile(ShaderType type);
    static std::string FindDXCExecutable();
    static std::vector<std::string> GetIncludeDirectories(const Path &shader_dir);
};

} // namespace Horizon::Backend
