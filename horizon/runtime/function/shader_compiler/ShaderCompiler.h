#pragma once

#include <string>
#include <vector>

#include <directx-dxc/d3d12shader.h>
#include <directx-dxc/dxcapi.h>

#include <runtime/core/utils/Definations.h>
#include <runtime/function/rhi/ShaderProgram.h>

namespace Horizon {
enum class ShaderTargetPlatform { SPIRV, DXIL };

// enum class ShaderTargetStage
//{
//	vs,
//	ps,
//	cs
// };

class ShaderCompiler {
  public:
    ShaderCompiler() noexcept;

    ~ShaderCompiler() noexcept;

    ShaderCompiler(const ShaderCompiler &rhs) noexcept = delete;

    ShaderCompiler &operator=(const ShaderCompiler &rhs) noexcept = delete;

    ShaderCompiler(ShaderCompiler &&rhs) noexcept = delete;

    ShaderCompiler &operator=(ShaderCompiler &&rhs) noexcept = delete;

  public:
    IDxcBlob *CompileFromFile(ShaderTargetPlatform platform, ShaderType type, const std::string &entry_point,
                              u32 compile_flags, std::string file_name);

    static std::vector<char> read_file(const std::string &path);

  private:
    void InitializeShaderCompiler();

  private:
    IDxcUtils *idxc_utils;
    ;
    IDxcCompiler3 *idxc_compiler;
};

} // namespace Horizon
