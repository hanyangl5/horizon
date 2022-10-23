#pragma once

#include <array>
#include <deque>
#include <vector>

#include <spirv_reflect.h>
#include <vulkan/vulkan.h>

#include <runtime/core/utils/Definations.h>

#include <runtime/function/rhi/RHIUtils.h>
#include <runtime/function/rhi/vulkan/VulkanBuffer.h>
#include <runtime/function/rhi/vulkan/VulkanSampler.h>
#include <runtime/function/rhi/vulkan/VulkanShader.h>
#include <runtime/function/rhi/vulkan/VulkanTexture.h>

namespace Horizon::Backend {

class Pipeline;
class VulkanPipeline;

struct DescriptorPoolSizeDesc {
    std::unordered_map<VkDescriptorType, u32> required_descriptor_count_per_type;
};

struct PipelineDescriptorSetResource {
    //std::array<u32, DESCRIPTOR_SET_UPDATE_FREQUENCIES> m_used_set_counter{0, 0, 0, 0};
    //std::array<DescriptorPoolSizeDesc, DESCRIPTOR_SET_UPDATE_FREQUENCIES> descriptor_pool_size_descs{};
    //std::array<u32, DESCRIPTOR_SET_UPDATE_FREQUENCIES> descriptor_pool_index{};
};

static constexpr std::array<u32, DESCRIPTOR_SET_UPDATE_FREQUENCIES> m_reserved_max_sets{
    1, 1, 16, 16}; // per batch / per draw descriptorpool can allocate 16 descriptorsets

class VulkanDescriptorSetAllocator {
  public:
    VulkanDescriptorSetAllocator(const VulkanRendererContext &context) noexcept;
    ~VulkanDescriptorSetAllocator() noexcept;

    VulkanDescriptorSetAllocator(const VulkanDescriptorSetAllocator &rhs) noexcept = delete;
    VulkanDescriptorSetAllocator &operator=(const VulkanDescriptorSetAllocator &rhs) noexcept = delete;
    VulkanDescriptorSetAllocator(VulkanDescriptorSetAllocator &&rhs) noexcept = delete;
    VulkanDescriptorSetAllocator &operator=(VulkanDescriptorSetAllocator &&rhs) noexcept = delete;

  public:
    void ResetDescriptorPool();

    VkDescriptorSetLayout GetVkDescriptorSetLayout(u64 key) const;

    // delay the pool creation and set allocation after all shader seted;
    void UpdateDescriptorPoolInfo(Pipeline *pipeline, const std::array<u64, DESCRIPTOR_SET_UPDATE_FREQUENCIES> &);

    void CreateDescriptorPool();

    void CreateDescriptorSetLayout(VulkanPipeline *pipeline);

  public:
    const VulkanRendererContext &m_context{};

    u64 m_empty_descriptor_set_layout_hash_key{};
    VkDescriptorSet m_empty_descriptor_set{};

    std::unordered_map<u64, VkDescriptorSetLayout> m_descriptor_set_layout_map{}; // cache exist layout

    std::unordered_map<Pipeline *, PipelineDescriptorSetResource> pipeline_descriptor_set_resources{};
    std::vector<DescriptorSet *> allocated_sets{};
    std::vector<VkDescriptorPool> m_descriptor_pools{};
    VkDescriptorPool m_temp_descriptor_pool{};

};

} // namespace Horizon::Backend