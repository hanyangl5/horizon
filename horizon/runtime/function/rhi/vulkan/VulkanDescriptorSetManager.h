#pragma once

#include <array>
#include <vector>

#include <spirv_reflect.h>
#include <vulkan/vulkan.h>

#include <runtime/core/utils/Definations.h>
#include <runtime/function/rhi/RHIUtils.h>
#include <runtime/function/rhi/Shader.h>
#include <runtime/function/rhi/vulkan/VulkanBuffer.h>
#include <runtime/function/rhi/vulkan/VulkanShader.h>

namespace Horizon::RHI {

class Pipeline;
class VulkanPipeline;

struct PipelineLayoutDesc {
  public:
    std::array<u64, 4> descriptor_set_hash_key{};
};

struct DescriptorSetValue {
    VkDescriptorSetLayout layout;
};

struct DescriptorPoolSizeDesc {
    struct DescriptorCount {
        u32 reserved = 128; // 1024 descriptor for each descriptor type
        u32 required = 0;   // descriptor required
    };
    std::unordered_map<VkDescriptorType, DescriptorCount> descriptor_type_map;
    u32 max_sets = 10;     // max set a pool can allocate
    u32 required_sets = 0; // set required
    bool recreate = true;  // need to recreate descriptor pool
};

struct DescriptorSetInfo {
    VkDescriptorSet set;
    // std::unordered_map<u32, VkWriteDescriptorSet> writes;
    static constexpr u32 MAX_BINDING_PER_DESCRIPTOR_SET = 16;
    std::array<VkWriteDescriptorSet, MAX_BINDING_PER_DESCRIPTOR_SET> writes;
};

struct PipelineDescriptorSetInfo {
    static constexpr u32 MAX_SET_COUNT_PER_PIPELINE = 4;
    std::array<DescriptorSetInfo, MAX_SET_COUNT_PER_PIPELINE> infos;
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
    void UpdatePipelineDescriptorSet(Pipeline *pipeline, ResourceUpdateFrequency frequency);

    VkDescriptorSetLayout FindLayout(u64 key) const;
    void AllocateDescriptorSets(VulkanPipeline *pipeline, ResourceUpdateFrequency frequency);

  public:
    void CreateDescriptorPool();
    PipelineLayoutDesc
    CreateDescriptorSetLayoutFromShader(::std::unordered_map<ShaderType, Shader *> &shader_map,
                                        PipelineType pipeline_type);
    // create layout for a single shader

    PipelineLayoutDesc GetGraphicsPipelineLayout(VulkanShader *vs, VulkanShader *ps);
    PipelineLayoutDesc GetComputePipelineLayout(VulkanShader *cs);

    void BindResource(Pipeline *pipeline, Buffer *buffer, ResourceUpdateFrequency freq, u32 binding);

    void InitEmptyDescriptorSet();

  public:
    const VulkanRendererContext &m_context;
    DescriptorPoolSizeDesc descriptor_pool_size_desc;
    VkDescriptorPool m_descriptor_pool = VK_NULL_HANDLE;

    std::unordered_map<u64, DescriptorSetValue> m_descriptor_set_layout_map; // cache exist layout

    std::unordered_map<Pipeline *, PipelineDescriptorSetInfo> m_pipeline_descriptors_map;

    VkDescriptorSet m_empty_descriptor_set{};
    u64 m_empty_descriptor_set_layout_hash_key{};
};

} // namespace Horizon::RHI