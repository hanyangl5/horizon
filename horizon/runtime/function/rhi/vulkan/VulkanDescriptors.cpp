#include <algorithm>
#include <runtime/function/rhi/vulkan/VulkanDescriptors.h>

namespace Horizon::RHI {

VulkanDescriptor::VulkanDescriptor(VkDevice device) noexcept
    : m_device(device) {

}

VulkanDescriptor::~VulkanDescriptor() {
    // TODO: destroy descriptor resources, descriptorsetlayout, descriptorpool
}
// TODO: reallocate global descriptorset per frame?
void VulkanDescriptor::AllocateDescriptors() noexcept {
    // create descriptorpool
    if (m_bindless_descriptor_pool == VK_NULL_HANDLE) {

        std::vector<VkDescriptorType> vk_types{};

        std::transform(
            m_bindless_descriptor_types.begin(),
            m_bindless_descriptor_types.end(), std::back_inserter(vk_types),
            [](DescriptorType type) { return ToVkDescriptorType(type); });

        std::vector<VkDescriptorPoolSize> pool_sizes{};

        std::transform(
            vk_types.begin(), vk_types.end(), std::back_inserter(pool_sizes),
            [](VkDescriptorType type) {
                return VkDescriptorPoolSize{type, m_k_max_binding_count};
            });

        VkDescriptorPoolCreateInfo pool_info{};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = 0;
        pool_info.poolSizeCount = static_cast<u32>(pool_sizes.size());
        pool_info.pPoolSizes = pool_sizes.data();
        pool_info.maxSets = static_cast<u32>(pool_sizes.size());

        CHECK_VK_RESULT(vkCreateDescriptorPool(m_device, &pool_info, nullptr,
                                               &m_bindless_descriptor_pool));

        VkDescriptorSetLayoutBinding binding{};
        binding.binding = 0;
        binding.descriptorCount = m_k_max_binding_count;
        binding.stageFlags = VK_SHADER_STAGE_ALL;

        VkDescriptorBindingFlags flag =
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;

        VkDescriptorSetLayoutBindingFlagsCreateInfo flag_info{};
        flag_info.sType =
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
        flag_info.bindingCount = 1;
        flag_info.pBindingFlags = &flag;

        VkDescriptorSetLayoutCreateInfo set_layout_info{};
        set_layout_info.sType =
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        set_layout_info.pNext = &flag_info;
        set_layout_info.bindingCount = 1;

        for (u32 i = 0; i < m_set_layouts.size(); i++) {
            binding.descriptorType = vk_types[i];
            set_layout_info.pBindings = &binding;
            CHECK_VK_RESULT(vkCreateDescriptorSetLayout(
                m_device, &set_layout_info, nullptr, &m_set_layouts[i]));
        }

        VkDescriptorSetVariableDescriptorCountAllocateInfoEXT count_info{
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO_EXT};

        count_info.descriptorSetCount = 1;
        count_info.pDescriptorCounts = &m_k_max_binding_count - 1; // ?

        VkDescriptorSetAllocateInfo set_info{};
        set_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        set_info.descriptorPool = m_bindless_descriptor_pool;
        set_info.descriptorSetCount = 1;
        set_info.pNext = &count_info;
        for (u32 i = 0; i < m_sets.size(); i++) {
            set_info.pSetLayouts = &m_set_layouts[i];
            CHECK_VK_RESULT(
                vkAllocateDescriptorSets(m_device, &set_info, &m_sets[i]));
        }
    }
}
void VulkanDescriptor::ResetDescriptorPool() noexcept {
    vkResetDescriptorPool(m_device, m_bindless_descriptor_pool, 0);
}
void VulkanDescriptor::Update() noexcept {

    vkUpdateDescriptorSets(m_device, descriptor_writes.size(),
                           descriptor_writes.data(), 0, nullptr);
}
} // namespace Horizon::RHI