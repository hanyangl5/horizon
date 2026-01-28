#pragma once

#include <runtime/rhi/pipeline.h>
#include <runtime/rhi/shader.h>
#include <runtime/rhi/vulkan/vulkandescriptorset.h>
#include <runtime/rhi/vulkan/vulkandescriptorsetallocator.h>
#include <runtime/rhi/vulkan/vulkanutils.h>

namespace Horizon::Backend {

class VulkanPipeline : public Pipeline {
  public:
    VulkanPipeline(const VulkanRendererContext &context, const GraphicsPipelineCreateInfo &create_info,
                   VulkanDescriptorSetAllocator &descriptor_set_manager) noexcept;
    VulkanPipeline(const VulkanRendererContext &context, const ComputePipelineCreateInfo &create_info,
                   VulkanDescriptorSetAllocator &descriptor_set_manager) noexcept;

    virtual ~VulkanPipeline() noexcept;
    VulkanPipeline(const VulkanPipeline &rhs) noexcept = delete;
    VulkanPipeline &operator=(const VulkanPipeline &rhs) noexcept = delete;
    VulkanPipeline(VulkanPipeline &&rhs) noexcept = delete;
    VulkanPipeline &operator=(VulkanPipeline &&rhs) noexcept = delete;

    void SetComputeShader(Shader *vs) override;

    void SetGraphicsShader(Shader *vs, Shader *ps) override;

    DescriptorSet *GetDescriptorSet(ResourceUpdateFrequency frequency) override;

    const RootSignatureDesc &GetRootSignatureDesc() const noexcept { return rsd; }

  private:
    void CreateGraphicsPipeline();
    void CreateComputePipeline();
    void CreatePipelineLayout();

  public:
    const VulkanRendererContext &m_context{};
    VulkanDescriptorSetAllocator &m_descriptor_set_allocator;
    VkPipeline m_pipeline{};
    VkPipelineLayout m_pipeline_layout{};
    VkPipelineLayoutDesc m_pipeline_layout_desc{};
    VkViewport view_port{};
    VkRect2D scissor{};
};

} // namespace Horizon::Backend