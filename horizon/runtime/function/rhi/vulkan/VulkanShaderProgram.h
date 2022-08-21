#pragma once

#include <runtime/function/rhi/RHIUtils.h>
#include <runtime/function/rhi/ShaderProgram.h>
#include <runtime/function/rhi/vulkan/VulkanUtils.h>
#include <runtime/function/rhi/ShaderReflection.h>

#include <d3d12shader.h>
#include <dxc/dxcapi.h>

namespace Horizon::RHI {
class VulkanShaderProgram : public ShaderProgram {
  public:
    VulkanShaderProgram(const VulkanRendererContext &context, ShaderType type,
                        std::vector<char>& spirv_code) noexcept;
    virtual ~VulkanShaderProgram() noexcept;
    VulkanShaderProgram(const VulkanShaderProgram &rhs) noexcept = delete;
    VulkanShaderProgram &operator=(const VulkanShaderProgram &rhs) noexcept = delete;
    VulkanShaderProgram(VulkanShaderProgram &&rhs) noexcept = delete;
    VulkanShaderProgram &operator=(VulkanShaderProgram &&rhs) noexcept = delete;

  public:
    const VulkanRendererContext &m_context;
    VkShaderModule m_shader_module{};
    // reflection data
    std::vector<char> m_spirv_code;
    PipelineReflection *p_reflection;
};

} // namespace Horizon::RHI