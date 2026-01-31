#pragma once

#include <rhi/enums.h>
#include <rhi/shader.h>
#include <rhi/vulkan/vulkan_utils.h>

#include <vulkan/vulkan.h>

#include <vector>

namespace Horizon::Backend
{

class VulkanShader : public Shader
{
  public:
    VulkanShader(const VulkanRendererContext &context, ShaderType type, std::vector<char> &spirv_code) noexcept;
    ~VulkanShader() noexcept override;
    VulkanShader(const VulkanShader &rhs) noexcept = delete;
    VulkanShader &operator=(const VulkanShader &rhs) noexcept = delete;
    VulkanShader(VulkanShader &&rhs) noexcept = delete;
    VulkanShader &operator=(VulkanShader &&rhs) noexcept = delete;

    const RootSignatureDesc *GetReflectionData() const noexcept override
    {
        return &m_reflection;
    }

    const VulkanRendererContext &m_context{};
    VkShaderModule m_shader_module{};

  private:
    RootSignatureDesc m_reflection{};
};

} // namespace Horizon::Backend