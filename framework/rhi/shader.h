#pragma once

#include <rhi/enums.h>

namespace Horizon::Backend
{

class Shader
{
  public:
    explicit Shader(ShaderType type) noexcept;
    virtual ~Shader() noexcept = default;

    Shader(const Shader &rhs) noexcept = delete;
    Shader &operator=(const Shader &rhs) noexcept = delete;
    Shader(Shader &&rhs) noexcept = delete;
    Shader &operator=(Shader &&rhs) noexcept = delete;

    ShaderType GetType() const noexcept;

    virtual const RootSignatureDesc *GetReflectionData() const noexcept
    {
        return nullptr;
    }

  protected:
    const ShaderType m_type{};
};

} // namespace Horizon::Backend