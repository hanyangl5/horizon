#pragma once

#include <runtime/function/rhi/Pipeline.h>
#include <runtime/function/rhi/ShaderProgram.h>
#include <runtime/function/rhi/vulkan/VulkanDescriptors.h>
#include <runtime/function/rhi/vulkan/VulkanUtils.h>

namespace Horizon::RHI {
class VulkanPipeline : public Pipeline {
  public:
    VulkanPipeline(const VulkanRendererContext &context,
                   const PipelineCreateInfo &pipeline_create_info,
                   VulkanDescriptor *descriptor) noexcept;
    ~VulkanPipeline() noexcept;
    void Create() noexcept;
    virtual void SetShader(ShaderProgram *shader_moudle) noexcept override;

  private:
    void CreateGraphicsPipeline() noexcept;
    void CreateComputePipeline() noexcept;
    void CreatePipelineLayout() noexcept;
    // void CreateRTPipeline() noexcept;
  public:
    const VulkanRendererContext &m_context;
    VkPipeline m_pipeline{};
    VkPipelineLayout m_pipeline_layout{};
    const VulkanDescriptor *m_descriptor{};
};

} // namespace Horizon::RHI