#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <rhi/enums.h>

namespace Horizon::Backend {

class ShaderCompiler {
  public:
    // Compile HLSL to SPIR-V using DXC
    // Returns true on success, false on failure
    static bool CompileHLSLToSPIRV(const std::filesystem::path &hlsl_path,
                                    const std::filesystem::path &output_spv_path,
                                    ShaderType shader_type,
                                    const std::filesystem::path &shader_dir,
                                    const std::filesystem::path &saved_shader_dir);

    // Check if shader needs recompilation
    // Returns true if source is newer than cached SPV or SPV doesn't exist
    static bool NeedsRecompilation(const std::filesystem::path &hlsl_path,
                                    const std::filesystem::path &spv_path);

  private:
    static std::string GetShaderProfile(ShaderType type);
    static std::string FindDXCExecutable();
    static std::vector<std::string> GetIncludeDirectories(const std::filesystem::path &shader_dir);
};

} // namespace Horizon::Backend
