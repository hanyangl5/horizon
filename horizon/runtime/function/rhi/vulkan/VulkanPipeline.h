#pragma once

#include <runtime/function/rhi/Pipeline.h>
#include <runtime/function/rhi/ShaderProgram.h>
#include <runtime/function/rhi/vulkan/VulkanDescriptorSetManager.h>
#include <runtime/function/rhi/vulkan/VulkanUtils.h>

namespace Horizon::RHI {
class VulkanPipeline : public Pipeline {
  public:
    VulkanPipeline(const VulkanRendererContext &context, const GraphicsPipelineCreateInfo &create_info,
                   VulkanDescriptorSetManager &descriptor_set_manager) noexcept;
    VulkanPipeline(const VulkanRendererContext &context, const ComputePipelineCreateInfo &create_info,
                   VulkanDescriptorSetManager &descriptor_set_manager) noexcept;
    ~VulkanPipeline() noexcept;

    const std::vector<VkDescriptorSet> &CreatePipelineResources() noexcept;

    void SetComputeShader(ShaderProgram *vs) noexcept override;
    void SetGraphicsShader(ShaderProgram *vs, ShaderProgram *ps) noexcept override;

  private:
    void CreateGraphicsPipeline() noexcept;
    void CreateComputePipeline() noexcept;
    void CreatePipelineLayout() noexcept;
    // void CreateRTPipeline() noexcept;
  public:
    const VulkanRendererContext &m_context;
    VkPipeline m_pipeline{};
    VkPipelineLayout m_pipeline_layout{};
    VulkanDescriptorSetManager &m_descriptor_set_manager;
    PipelineLayoutDesc m_pipeline_layout_desc;
};

} // namespace Horizon::RHI