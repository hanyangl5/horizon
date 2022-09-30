#pragma once

#include <array>
#include <vector>
#include <deque>

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

struct PipelineDescriptorSetResource {
    std::array<u64, DESCRIPTOR_SET_UPDATE_FREQUENCIES> layout_hash_key;
    std::array<u32, DESCRIPTOR_SET_UPDATE_FREQUENCIES> m_used_set_counter{0, 0, 0, 0};
    std::array<std::vector<VkDescriptorSet>, DESCRIPTOR_SET_UPDATE_FREQUENCIES> allocated_sets;
};

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

    VkDescriptorSetLayout FindLayout(u64 key) const;

    // delay the pool creation and set allocation after all shader seted;
    void UpdateDescriptorPoolInfo(Pipeline* pipeline, const std::array<u64, DESCRIPTOR_SET_UPDATE_FREQUENCIES>&);

    void CreateDescriptorPool();

    PipelineLayoutDesc CreateDescriptorSetLayoutFromShader(::std::unordered_map<ShaderType, Shader *> &shader_map,
                                                           PipelineType pipeline_type);

    void ReflectDescriptorSetLayoutFromShader(
        VulkanShader *shader, std::array<VkDescriptorSetLayoutCreateInfo, DESCRIPTOR_SET_UPDATE_FREQUENCIES> &layout_create_in,
        std::array<std::array<VkDescriptorSetLayoutBinding, MAX_BINDING_PER_DESCRIPTOR_SET>, DESCRIPTOR_SET_UPDATE_FREQUENCIES> &layout_bindings);
  public:
    const VulkanRendererContext &m_context;

    std::array<u32, DESCRIPTOR_SET_UPDATE_FREQUENCIES> m_reserved_max_sets{1, 1, 16, 16}; // per batch / per draw descriptor set are deafult has 16 descriptors
    std::array<DescriptorPoolSizeDesc, DESCRIPTOR_SET_UPDATE_FREQUENCIES> descriptor_pool_size_descs{};

    u64 m_empty_descriptor_set_layout_hash_key{};
    VkDescriptorSet m_empty_descriptor_set{};

    std::unordered_map<u64, DescriptorSetLayout> m_descriptor_set_layout_map{}; // cache exist layout

    std::array<std::deque<VkDescriptorPool>, DESCRIPTOR_SET_UPDATE_FREQUENCIES> m_descriptor_pools{};

    std::unordered_map <Pipeline *, PipelineDescriptorSetResource> pipeline_descriptor_set_resources;

    bool pool_created = false;
    // ----------
};

} // namespace Horizon::RHI