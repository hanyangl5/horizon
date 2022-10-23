#pragma once

#include <runtime/function/rhi/RHIUtils.h>
#include <runtime/function/rhi/Shader.h>
#include <runtime/function/rhi/vulkan/VulkanUtils.h>

#include <d3d12shader.h>
#include <dxc/dxcapi.h>

namespace Horizon::Backend {
class VulkanShader : public Shader {
  public:
    VulkanShader(const VulkanRendererContext &context, ShaderType type,
                        std::vector<char>& spirv_code, const std::filesystem::path& rsd_path) noexcept;
    virtual ~VulkanShader() noexcept;
    VulkanShader(const VulkanShader &rhs) noexcept = delete;
    VulkanShader &operator=(const VulkanShader &rhs) noexcept = delete;
    VulkanShader(VulkanShader &&rhs) noexcept = delete;
    VulkanShader &operator=(VulkanShader &&rhs) noexcept = delete;

  public:
    const VulkanRendererContext &m_context{};
    VkShaderModule m_shader_module{};
    std::vector<char> m_spirv_code{}; // TODO: remove this after generate push constant in rsd
};

} // namespace Horizon::Backend