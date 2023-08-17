#pragma once

#include <vulkan/vulkan.h>

#include <runtime/core/utils/definations.h>

#include <runtime/rhi/Enums.h>
#include <runtime/rhi/vulkan/VulkanBuffer.h>
#include <runtime/rhi/vulkan/VulkanSampler.h>
#include <runtime/rhi/vulkan/VulkanShader.h>
#include <runtime/rhi/vulkan/VulkanTexture.h>

namespace Horizon::Backend {

class Pipeline;
class VulkanPipeline;

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
    void CreateBindlessDescriptorPool();

    void CreateDescriptorSetLayout(VulkanPipeline *pipeline);

  public:
    const VulkanRendererContext &m_context{};

    u64 m_empty_descriptor_set_layout_hash_key{};
    VkDescriptorSet m_empty_descriptor_set{};

    std::unordered_map<u64, VkDescriptorSetLayout> m_descriptor_set_layout_map{}; // cache exist layout

    std::vector<DescriptorSet *> allocated_sets{};
    VkDescriptorPool m_temp_descriptor_pool{};

    // bindless
    static constexpr u32 k_max_bindless_resources = 65536;

    VkDescriptorPool m_bindless_descriptor_pool{};
};

} // namespace Horizon::Backend