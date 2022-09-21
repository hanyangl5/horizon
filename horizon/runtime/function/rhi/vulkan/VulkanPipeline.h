#pragma once

#include <runtime/function/rhi/Pipeline.h>
#include <runtime/function/rhi/Shader.h>
#include <runtime/function/rhi/vulkan/VulkanUtils.h>
#include <runtime/function/rhi/vulkan/VulkanDescriptorSet.h>
#include <runtime/function/rhi/vulkan/VulkanDescriptorSetAllocator.h>

namespace Horizon::RHI {

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

    void CreatePipelineResources();

    void SetComputeShader(Shader *vs) override;

    void SetGraphicsShader(Shader *vs, Shader *ps) override;

    Resource<DescriptorSet> GetDescriptorSet(ResourceUpdateFrequency frequency) override; 

  private:
    void CreateGraphicsPipeline();
    void CreateComputePipeline();
    void CreatePipelineLayout();
    
    void ReflectPushConstants(VulkanShader * shader, PipelineLayoutDesc& layout_desc);

    // void CreateRTPipeline() noexcept;  
  public:
    const VulkanRendererContext &m_context;
    VulkanDescriptorSetAllocator &m_descriptor_set_allocator;
    VkPipeline m_pipeline{};
    VkPipelineLayout m_pipeline_layout{};
    PipelineLayoutDesc m_pipeline_layout_desc;
    VkViewport view_port{};
    VkRect2D scissor{};
};

} // namespace Horizon::RHI