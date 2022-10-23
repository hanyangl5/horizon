#pragma once

#include <string>
#include <filesystem>

#include <runtime/function/rhi/RHIUtils.h>


namespace Horizon::Backend {
class Shader {
  public:
    Shader(ShaderType type, const std::filesystem::path &rsd_path) noexcept;
    virtual ~Shader() noexcept = default;

    Shader(const Shader &rhs) noexcept = delete;
    Shader &operator=(const Shader &rhs) noexcept = delete;
    Shader(Shader &&rhs) noexcept = delete;
    Shader &operator=(Shader &&rhs) noexcept = delete;

    ShaderType GetType() const noexcept;

    const std::filesystem::path &GetRootSignatureDescriptionPath() const noexcept { return m_rsd_path; }
  protected:
    const ShaderType m_type{};
    const std::filesystem::path m_rsd_path; // lazy read


};

} // namespace Horizon::Backend