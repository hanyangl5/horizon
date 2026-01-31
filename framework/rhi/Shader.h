#pragma once

#include <rhi/enums.h>

namespace Horizon::Backend
{

class Shader
{
  public:
    explicit Shader(ShaderType type, const char *entry_point) noexcept;
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
    const char *GetEntryPoint() const noexcept
    {
        return m_entry_point.c_str();
    }

  protected:
    const ShaderType m_type{};
    std::string m_entry_point;
};

} // namespace Horizon::Backend