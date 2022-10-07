#pragma once

#include <string>

#include <runtime/function/rhi/RHIUtils.h>


namespace Horizon::RHI {
class Shader {
  public:
    Shader(ShaderType type, const std::vector<char>& rsd_code) noexcept;
    virtual ~Shader() noexcept = default;

    Shader(const Shader &rhs) noexcept = delete;
    Shader &operator=(const Shader &rhs) noexcept = delete;
    Shader(Shader &&rhs) noexcept = delete;
    Shader &operator=(Shader &&rhs) noexcept = delete;

    ShaderType GetType() const noexcept;
    const std::string &GetEntryPoint() const noexcept;

  protected:
    const ShaderType m_type{};
    std::string m_entry_point{};
    RootSignatureDesc rsd;
    std::array<u32, DESCRIPTOR_SET_UPDATE_FREQUENCIES> vk_binding_count{}; // TODO: any smarter way?

};

} // namespace Horizon::RHI