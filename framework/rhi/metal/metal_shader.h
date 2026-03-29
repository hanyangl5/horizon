#pragma once

#include <rhi/metal/metal_utils.h>

namespace Horizon::Backend
{

class MetalShader final : public Shader
{
  public:
    MetalShader(ShaderType type, const char *entry_point, MTL::Library *library, MTL::Function *function) noexcept;
    ~MetalShader() noexcept override;
    MetalShader(const MetalShader &rhs) noexcept = delete;
    MetalShader &operator=(const MetalShader &rhs) noexcept = delete;
    MetalShader(MetalShader &&rhs) noexcept = delete;
    MetalShader &operator=(MetalShader &&rhs) noexcept = delete;

    MTL::Library *m_library{};
    MTL::Function *m_function{};
};

} // namespace Horizon::Backend
