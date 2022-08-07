#pragma once
#include <runtime/core/utils/Definations.h>
#include <runtime/function/rhi/RHIUtils.h>
#include <runtime/function/rhi/vulkan/VulkanUtils.h>
#include <runtime/function/rhi/ShaderProgram.h>

#include <array>

namespace Horizon::RHI {

class VulkanDescriptorSetManager {
  public:
    VulkanDescriptorSetManager(const VulkanRendererContext &context) noexcept;
    ~VulkanDescriptorSetManager() noexcept;
    void AllocateDescriptors() noexcept;
    void ResetDescriptorPool() noexcept;
    void Update() noexcept;

  public:
    std::vector<u32> CreateLayouts(std::unordered_map<ShaderType, ShaderProgram *>& shader_map, PipelineType pipeline_type) noexcept;

  public:
    const VulkanRendererContext &m_context;
    std::unordered_map<u64, VkDescriptorSet> descriptor_set_map;
};

} // namespace Horizon::RHI