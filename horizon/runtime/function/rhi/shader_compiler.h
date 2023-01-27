/*****************************************************************/ /**
 * \file   FILE_NAME
 * \brief  BRIEF
 * 
 * \author XXX
 * \date   XXX
 *********************************************************************/

#pragma once

// standard libraries

// third party libraries
#include <directx-dxc/d3d12shader.h>
#include <directx-dxc/dxcapi.h>

// project headers
#include <runtime/core/singleton/public_singleton.h>
#include <runtime/function/rhi/rhi_utils.h>
#include <runtime/function/rhi/shader.h>

namespace Horizon {

class ShaderCompiler : public Singleton<ShaderCompiler> {
  public:
    ShaderCompiler() noexcept;
    virtual ~ShaderCompiler() noexcept;

    constexpr ShaderCompiler(const ShaderCompiler &rhs) noexcept = delete;
    constexpr ShaderCompiler &operator=(const ShaderCompiler &rhs) noexcept = delete;
    constexpr ShaderCompiler(ShaderCompiler &&rhs) noexcept = delete;
    constexpr ShaderCompiler &operator=(ShaderCompiler &&rhs) noexcept = delete;

    void CompileFromFile(const std::filesystem::path &path, const ShaderCompilationArgs &compile_args);
    void Compile(const Container::Array<char> &blob, const ShaderCompilationArgs &compile_args);

  private:
    IDxcUtils *idxc_utils;
    IDxcCompiler3 *idxc_compiler;
    IDxcIncludeHandler *idxc_include_handler;
};

} // namespace Horizon