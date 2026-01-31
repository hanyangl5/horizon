#pragma once

#include <core/path.h>
#include <string>
#include <vector>

#include <rhi/enums.h>

namespace Horizon::Backend
{

class ShaderCompiler
{
  public:
    // Compile HLSL to SPIR-V using DXC
    // Returns true on success, false on failure
    static bool CompileHLSLToSPIRV(const Path &hlsl_path, const Path &output_spv_path, ShaderType shader_type,
                                   const Path &shader_dir, const Path &saved_shader_dir, const char *entry_point);

    // Check if shader needs recompilation
    // Returns true if source is newer than cached SPV or SPV doesn't exist
    static bool NeedsRecompilation(const Path &hlsl_path, const Path &spv_path);

  private:
    static std::string GetShaderProfile(ShaderType type);
    static std::string FindDXCExecutable();
    static std::vector<std::string> GetIncludeDirectories(const Path &shader_dir);
};

} // namespace Horizon::Backend
