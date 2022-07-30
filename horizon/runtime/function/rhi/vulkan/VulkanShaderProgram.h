#pragma once

#include <runtime/function/rhi/RHIUtils.h>
#include <runtime/function/rhi/ShaderProgram.h>
#include <runtime/function/rhi/vulkan/VulkanUtils.h>

#include <spirv_reflect.h>
#include <d3d12shader.h>
#include <dxc/dxcapi.h>

namespace Horizon::RHI {
class VulkanShaderProgram : public ShaderProgram {
  public:
    VulkanShaderProgram(const VulkanRendererContext& context, ShaderType type,
                        const std::string &entry_point,
                        IDxcBlob *shader_byte_code) noexcept;
    virtual ~VulkanShaderProgram() noexcept;
    // virtual void* GetBufferPointer() const noexcept override;
    // virtual u64 GetBufferSize() const noexcept override;
    void ReflectDescriptorSetLayout(void* spirv,u32 size);
  public:
    const VulkanRendererContext &m_context;
    VkShaderModule m_shader_module{};
    // reflection data
    std::vector<SpvReflectDescriptorSet *> sets;
};

} // namespace Horizon::RHI