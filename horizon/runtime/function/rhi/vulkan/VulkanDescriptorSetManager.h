#pragma once
#include <array>

#include <spirv_reflect.h>

#include <runtime/core/utils/Definations.h>
#include <runtime/function/rhi/RHIUtils.h>
#include <runtime/function/rhi/ShaderProgram.h>
#include <runtime/function/rhi/vulkan/VulkanShaderProgram.h>
#include <runtime/function/rhi/vulkan/VulkanUtils.h>

namespace Horizon::RHI {
struct PipelineLayoutDesc {
  public:
    std::vector<u64> descriptor_set_hash_key;
    std::vector<u32> set_index;
    std::vector<VkDescriptorSet> sets;
};

struct DescriptorSetValue {
    VkDescriptorSetLayout layout;
    // VkDescriptorSet set;
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

class VulkanDescriptorSetManager {
  public:
    VulkanDescriptorSetManager(const VulkanRendererContext &context) noexcept;
    ~VulkanDescriptorSetManager() noexcept;
    VulkanDescriptorSetManager(const VulkanDescriptorSetManager &rhs) noexcept = delete;
    VulkanDescriptorSetManager &operator=(const VulkanDescriptorSetManager &rhs) noexcept = delete;
    VulkanDescriptorSetManager(VulkanDescriptorSetManager &&rhs) noexcept = delete;
    VulkanDescriptorSetManager &operator=(VulkanDescriptorSetManager &&rhs) noexcept = delete;

    void ResetDescriptorPool();
    void Update();

    VkDescriptorSetLayout FindLayout(u64 key) const;
    std::vector<VkDescriptorSet> AllocateDescriptorSets(const PipelineLayoutDesc &layout_desc);

  public:
    // std::vector<SpvReflectDescriptorSet *>
    // ReflectDescriptorSetLayout(void *spirv, u32 size);
    void CreateDescriptorPool();
    PipelineLayoutDesc CreateLayouts(std::unordered_map<ShaderType, ShaderProgram *> &shader_map,
                                     PipelineType pipeline_type);
    // create layout for a single shader

    PipelineLayoutDesc GetGraphicsPipelineLayout(VulkanShaderProgram *vs, VulkanShaderProgram *ps);
    PipelineLayoutDesc GetComputePipelineLayout(VulkanShaderProgram *cs);

  public:
    const VulkanRendererContext &m_context;
    DescriptorPoolSizeDesc descriptor_pool_size_desc;
    VkDescriptorPool m_descriptor_pool = VK_NULL_HANDLE;
    std::unordered_map<u64, DescriptorSetValue> m_descriptor_set_layout_map; // cache exist layout

    // std::vector<DescriptorSetInfo> layouts;
    std::vector<VkWriteDescriptorSet> descriptor_writes;
};

} // namespace Horizon::RHI