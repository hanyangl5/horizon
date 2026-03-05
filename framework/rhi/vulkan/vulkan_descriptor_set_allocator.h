#pragma once

#include <volk.h>

#include <core/definations.h>

#include <rhi/enums.h>
#include <rhi/vulkan/vulkan_buffer.h>
#include <rhi/vulkan/vulkan_descriptor_set.h>
#include <rhi/vulkan/vulkan_sampler.h>
#include <rhi/vulkan/vulkan_shader.h>
#include <rhi/vulkan/vulkan_texture.h>

#include <vector>
namespace Horizon::Backend
{

class Pipeline;
class VulkanPipeline;

class VulkanDescriptorSetAllocator
{
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
    // (deprecated, kept for compatibility)

    void CreateDescriptorPool();
    void CreateBindlessDescriptorPool();

    void CreateDescriptorSetLayout(VulkanPipeline *pipeline);

    VulkanDescriptorSet *GetDescriptorSet(VulkanPipeline *pipeline);
    VulkanDescriptorSet *GetBindlessDescriptorSet(VulkanPipeline *pipeline);
    void ReleaseDescriptorSets(VulkanPipeline *pipeline);

  private:
    void InitializeUpdateAfterBindLimits();
    void InitializeDescriptorIndexingFeatures();
    u32 GetUpdateAfterBindTypeLimit(VkDescriptorType type) const;
    bool CheckBindlessFeatureSupport(const std::vector<VkDescriptorSetLayoutBinding> &bindings,
                                     bool requires_variable_descriptor) const;

    std::unordered_map<VkDescriptorType, u32> BuildPerSetTypeCounts(
        const std::unordered_map<std::string, DescriptorDesc> &descriptors) const;
    bool CreateDefaultPool(const std::unordered_map<VkDescriptorType, u32> &per_set_type_counts, u32 max_sets,
                           VkDescriptorPool &out_pool) const;
    bool EnsureBindlessPool();
    bool RecreateBindlessPool(bool grow_pool);
    bool RebindSharedBindlessSets();

    struct DefaultSetAllocation
    {
        VulkanDescriptorSet *set{};
        size_t pool_index{};
    };

    struct DefaultPoolState
    {
        VkDescriptorPool pool{};
        u32 max_sets{};
        std::unordered_map<VkDescriptorType, u32> per_set_type_counts{};
    };

    struct BindlessLayoutMeta
    {
        std::unordered_map<std::string, DescriptorDesc> write_descs{};
        u32 variable_descriptor_count{1};
    };

    struct BindlessSetState
    {
        VulkanDescriptorSet *set{};
        u32 variable_descriptor_count{1};
    };

  public:
    const VulkanRendererContext &m_context{};

    u64 m_empty_descriptor_set_layout_hash_key{};
    VkDescriptorSet m_empty_descriptor_set{};

    std::unordered_map<u64, VkDescriptorSetLayout> m_descriptor_set_layout_map{}; // cache exist layout
    std::unordered_map<void *, DefaultSetAllocation> allocated_descriptorsets;
    std::unordered_map<u64, BindlessSetState> allocated_bindless_descriptorsets;
    std::vector<DefaultPoolState> m_default_pools{};

    u32 m_max_uab_uniform_buffers{1};
    u32 m_max_uab_storage_buffers{1};
    u32 m_max_uab_samplers{1};
    u32 m_max_uab_sampled_images{1};
    u32 m_max_uab_storage_images{1};
    u32 m_max_uab_descriptors_all_pools{0};
    VkPhysicalDeviceDescriptorIndexingFeatures m_descriptor_indexing_features{};
    std::unordered_map<u64, BindlessLayoutMeta> m_bindless_layout_meta{};
    std::unordered_map<void *, u64> m_pipeline_bindless_layout_map{};
    std::unordered_map<u64, u32> m_bindless_layout_ref_count{};
    u32 m_bindless_pool_max_sets{1};

    VkDescriptorPool m_bindless_descriptor_pool{};
};

} // namespace Horizon::Backend
