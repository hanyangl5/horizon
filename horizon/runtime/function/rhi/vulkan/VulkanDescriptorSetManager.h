#pragma once

#include <array>
#include <vector>

#include <spirv_reflect.h>
#include <vulkan/vulkan.h>

#include <runtime/core/utils/Definations.h>

#include <runtime/function/rhi/RHIUtils.h>
#include <runtime/function/rhi/vulkan/VulkanBuffer.h>
#include <runtime/function/rhi/vulkan/VulkanTexture.h>
#include <runtime/function/rhi/vulkan/VulkanShader.h>
#include <runtime/function/rhi/vulkan/VulkanSampler.h>

namespace Horizon::RHI {

class Pipeline;
class VulkanPipeline;

struct DescriptorSetLayout {
    VkDescriptorSetLayout layout;
};

struct DescriptorPoolSizeDesc {
    std::unordered_map<VkDescriptorType, u32> required_descriptor_count_per_type;
};

class VulkanDescriptorSetManager {
  public:
    VulkanDescriptorSetManager(const VulkanRendererContext &context) noexcept;
    ~VulkanDescriptorSetManager() noexcept;

    VulkanDescriptorSetManager(const VulkanDescriptorSetManager &rhs) noexcept = delete;
    VulkanDescriptorSetManager &operator=(const VulkanDescriptorSetManager &rhs) noexcept = delete;
    VulkanDescriptorSetManager(VulkanDescriptorSetManager &&rhs) noexcept = delete;
    VulkanDescriptorSetManager &operator=(VulkanDescriptorSetManager &&rhs) noexcept = delete;

    void ResetDescriptorPool();

    VkDescriptorSetLayout FindLayout(u64 key) const;

  public:
    void CreateDescriptorPool(const std::array<u64, DESCRIPTOR_SET_UPDATE_FREQUENCIES>&);
    PipelineLayoutDesc CreateDescriptorSetLayoutFromShader(::std::unordered_map<ShaderType, Shader *> &shader_map,
                                                           PipelineType pipeline_type);

    void ReflectDescriptorSetLayoutFromShader(
        VulkanShader *shader, std::array<VkDescriptorSetLayoutCreateInfo, DESCRIPTOR_SET_UPDATE_FREQUENCIES> &layout_create_in,
        std::array<std::array<VkDescriptorSetLayoutBinding, MAX_BINDING_PER_DESCRIPTOR_SET>, DESCRIPTOR_SET_UPDATE_FREQUENCIES> &layout_bindings);

  public:
    const VulkanRendererContext &m_context;

    std::array<u32, DESCRIPTOR_SET_UPDATE_FREQUENCIES> m_reserved_max_sets{1, 1, 1, 1};
    std::array<u32, DESCRIPTOR_SET_UPDATE_FREQUENCIES> m_used_set_counter{0, 0, 0, 0};

    std::array<DescriptorPoolSizeDesc, DESCRIPTOR_SET_UPDATE_FREQUENCIES> descriptor_pool_size_descs{};
    u64 m_empty_descriptor_set_layout_hash_key{};
    std::unordered_map<u64, DescriptorSetLayout> m_descriptor_set_layout_map{}; // cache exist layout

    VkDescriptorSet m_empty_descriptor_set{};
    std::array<VkDescriptorPool, DESCRIPTOR_SET_UPDATE_FREQUENCIES> m_descriptor_pools{};

    struct AllocatedDescriptorSet {
        std::vector<VkDescriptorSet> sets;
    };
    std::array<AllocatedDescriptorSet, DESCRIPTOR_SET_UPDATE_FREQUENCIES> allocated_sets{};
    // ----------
};

} // namespace Horizon::RHI