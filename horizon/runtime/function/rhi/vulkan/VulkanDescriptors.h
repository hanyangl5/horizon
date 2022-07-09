#pragma once
#include <runtime/core/utils/Definations.h>
#include <runtime/function/rhi/RHIUtils.h>

#include <array>

namespace Horizon::RHI {

class VulkanDescriptor {
  public:
    VulkanDescriptor(VkDevice device) noexcept;
    ~VulkanDescriptor() noexcept;
    void AllocateDescriptors() noexcept;
    void ResetDescriptorPool() noexcept;
    void Update() noexcept;
  public:
    static constexpr u32 m_k_bindless_descriptor_type_count{4};
    static constexpr u32 m_k_max_binding_count{1024};
    std::array<VkDescriptorType, m_k_bindless_descriptor_type_count>
        m_bindless_descriptor_types{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                    VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                    VK_DESCRIPTOR_TYPE_STORAGE_IMAGE};
    VkDevice m_device{};
    VkDescriptorPool m_bindless_descriptor_pool{};
    std::array<VkDescriptorSetLayout, m_k_bindless_descriptor_type_count>
        m_set_layouts{};
    std::array<VkDescriptorSet, m_k_bindless_descriptor_type_count> m_sets{};

    std::array<VkWriteDescriptorSet, m_k_bindless_descriptor_type_count>
        descriptor_writes{};
};

} // namespace Horizon::RHI