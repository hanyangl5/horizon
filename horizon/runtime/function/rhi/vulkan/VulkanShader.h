#pragma once

#include <runtime/function/rhi/RHIUtils.h>
#include <runtime/function/rhi/Shader.h>
#include <runtime/function/rhi/vulkan/VulkanUtils.h>

#include <d3d12shader.h>
#include <dxc/dxcapi.h>

namespace Horizon::RHI {
class VulkanShader : public Shader {
  public:
    VulkanShader(const VulkanRendererContext &context, ShaderType type,
                        std::vector<char>& spirv_code, std::vector<char>& sld_code) noexcept;
    virtual ~VulkanShader() noexcept;
    VulkanShader(const VulkanShader &rhs) noexcept = delete;
    VulkanShader &operator=(const VulkanShader &rhs) noexcept = delete;
    VulkanShader(VulkanShader &&rhs) noexcept = delete;
    VulkanShader &operator=(VulkanShader &&rhs) noexcept = delete;

    RootSignatureDesc &GetRootSignatureDesc()  { return rsd; }
    std::array<u32, DESCRIPTOR_SET_UPDATE_FREQUENCIES> &GetVkMaxBindingIndex() { return vk_binding_count; }
  public:
    const VulkanRendererContext &m_context;
    VkShaderModule m_shader_module{};
    std::vector<char> m_spirv_code; // TODO: remove this after generate push constant in rsd
};

} // namespace Horizon::RHI