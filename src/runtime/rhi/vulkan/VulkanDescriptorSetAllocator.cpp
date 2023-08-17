#include "VulkanDescriptorSetAllocator.h"

#include <algorithm>

#define SPIRV_REFLECT_USE_SYSTEM_SPIRV_H
#include <spirv_reflect.h>

#include <runtime/rhi/Enums.h>
#include <runtime/rhi/ResourceCache.h>
#include <runtime/rhi/vulkan/VulkanPipeline.h>

namespace Horizon::Backend {

VulkanDescriptorSetAllocator::VulkanDescriptorSetAllocator(const VulkanRendererContext &context) noexcept
    : m_context(context) {
    // create empty layout
    if (m_empty_descriptor_set == VK_NULL_HANDLE || m_empty_descriptor_set_layout_hash_key == 0) {

        VkDescriptorSetLayoutCreateInfo set_layout_create_info{};
        set_layout_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;

        set_layout_create_info.flags = 0;
        set_layout_create_info.pNext = nullptr;
        set_layout_create_info.bindingCount = 0;
        set_layout_create_info.pBindings = nullptr;

        VkDescriptorSetLayout layout;
        CHECK_VK_RESULT(vkCreateDescriptorSetLayout(m_context.device, &set_layout_create_info, nullptr, &layout));

        m_empty_descriptor_set_layout_hash_key = std::hash<VkDescriptorSetLayoutCreateInfo>{}(set_layout_create_info);
        m_descriptor_set_layout_map.emplace(m_empty_descriptor_set_layout_hash_key, layout);
    }
}

void VulkanDescriptorSetAllocator::CreateDescriptorSetLayout(VulkanPipeline *pipeline) {
    auto &rsd = pipeline->GetRootSignatureDesc();

    std::array<std::vector<VkDescriptorSetLayoutBinding>, DESCRIPTOR_SET_UPDATE_FREQUENCIES> bindings{};

    for (u32 freq = 0; freq < DESCRIPTOR_SET_UPDATE_FREQUENCIES; freq++) {
        if (ResourceUpdateFrequency::BINDLESS == static_cast<ResourceUpdateFrequency>(freq)) {
            continue;
        }
        bindings[freq].reserve(rsd.descriptors[freq].size());
        for (auto &[name, descriptor] : rsd.descriptors[freq]) {
            VkDescriptorSetLayoutBinding binding{};
            binding.binding = descriptor.vk_binding;
            binding.descriptorCount = 1;
            binding.stageFlags = VK_SHADER_STAGE_ALL;
            binding.descriptorType = util_to_vk_descriptor_type(descriptor.type);
            bindings[freq].push_back(binding);
        }
    }

    std::array<VkDescriptorSetLayoutCreateInfo, DESCRIPTOR_SET_UPDATE_FREQUENCIES> layout_create_infos{};

    // create layouts
    for (u32 freq = 0; freq < bindings.size(); freq++) {
        if (ResourceUpdateFrequency::BINDLESS == static_cast<ResourceUpdateFrequency>(freq)) {
            continue;
        }
        // empty descriptor set
        if (bindings[freq].empty()) {
            pipeline->m_pipeline_layout_desc.descriptor_set_hash_key[freq] = m_empty_descriptor_set_layout_hash_key;
        } else {
            layout_create_infos[freq].sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            layout_create_infos[freq].bindingCount = static_cast<u32>(bindings[freq].size());
            layout_create_infos[freq].pBindings = bindings[freq].data();
            u64 key = std::hash<VkDescriptorSetLayoutCreateInfo>{}(layout_create_infos[freq]);

            auto res = m_descriptor_set_layout_map.find(key);

            if (res == m_descriptor_set_layout_map.end()) {
                VkDescriptorSetLayout layout;
                CHECK_VK_RESULT(
                    vkCreateDescriptorSetLayout(m_context.device, &layout_create_infos[freq], nullptr, &layout));
                m_descriptor_set_layout_map.emplace(key, layout);
            } else {
                LOG_DEBUG("layout exist");
            }
            pipeline->m_pipeline_layout_desc.descriptor_set_hash_key[freq] = key;
        }
    }

    // bindless layout

    u32 bindless_freq = static_cast<u32>(ResourceUpdateFrequency::BINDLESS);

    bindings[bindless_freq].reserve(rsd.descriptors[bindless_freq].size());
    for (auto &[name, descriptor] : rsd.descriptors[bindless_freq]) {
        VkDescriptorSetLayoutBinding binding{};
        binding.binding = descriptor.vk_binding;
        binding.descriptorCount = k_max_bindless_resources;
        binding.stageFlags = VK_SHADER_STAGE_ALL;
        binding.descriptorType = util_to_vk_descriptor_type(descriptor.type);
        bindings[bindless_freq].push_back(binding);
    }

    // binding flags
    VkDescriptorSetLayoutBindingFlagsCreateInfoEXT set_layout_binding_flags{};
    set_layout_binding_flags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT;
    set_layout_binding_flags.bindingCount = static_cast<u32>(bindings[bindless_freq].size());
    std::vector<VkDescriptorBindingFlags> flags(bindings[bindless_freq].size(),
                                                VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT |
                                                    VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT |
                                                    VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT);
    set_layout_binding_flags.pBindingFlags = flags.data();

    // layout create info
    VkDescriptorSetLayoutCreateInfo bindless_descriptorset_layout_create_info{};
    bindless_descriptorset_layout_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    bindless_descriptorset_layout_create_info.bindingCount = static_cast<u32>(bindings[bindless_freq].size());
    bindless_descriptorset_layout_create_info.pBindings = bindings[bindless_freq].data();
    bindless_descriptorset_layout_create_info.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT;
    bindless_descriptorset_layout_create_info.pNext = &set_layout_binding_flags;

    u64 key = std::hash<VkDescriptorSetLayoutCreateInfo>{}(bindless_descriptorset_layout_create_info);

    auto res = m_descriptor_set_layout_map.find(key);

    if (res == m_descriptor_set_layout_map.end()) {
        VkDescriptorSetLayout layout;
        CHECK_VK_RESULT(vkCreateDescriptorSetLayout(m_context.device, &bindless_descriptorset_layout_create_info,
                                                    nullptr, &layout));
        m_descriptor_set_layout_map.emplace(key, layout);
    } else {
        LOG_DEBUG("layout exist");
    }
    pipeline->m_pipeline_layout_desc.descriptor_set_hash_key[bindless_freq] = key;
}

VulkanDescriptorSetAllocator::~VulkanDescriptorSetAllocator() noexcept {
    for (auto &layout : m_descriptor_set_layout_map) {
        vkDestroyDescriptorSetLayout(m_context.device, layout.second, nullptr);
    }

    vkDestroyDescriptorPool(m_context.device, m_temp_descriptor_pool, nullptr);
    if (m_bindless_descriptor_pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(m_context.device, m_bindless_descriptor_pool, nullptr);
    }
}

void VulkanDescriptorSetAllocator::CreateDescriptorPool() {
    std::array<VkDescriptorType, 5> types{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                          VK_DESCRIPTOR_TYPE_SAMPLER, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                          VK_DESCRIPTOR_TYPE_STORAGE_IMAGE};

    std::vector<VkDescriptorPoolSize> pool_sizes;
    for (auto type : types) {
        pool_sizes.push_back(VkDescriptorPoolSize{type, 2048});
    }

    VkDescriptorPoolCreateInfo pool_create_info{};
    pool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_create_info.pNext = nullptr;
    pool_create_info.flags = 0;

    pool_create_info.maxSets = 2048;
    pool_create_info.poolSizeCount = static_cast<u32>(pool_sizes.size());
    pool_create_info.pPoolSizes = pool_sizes.data();

    CHECK_VK_RESULT(vkCreateDescriptorPool(m_context.device, &pool_create_info, nullptr, &m_temp_descriptor_pool));
}

void VulkanDescriptorSetAllocator::CreateBindlessDescriptorPool() {
    std::array<VkDescriptorType, 5> types{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                          VK_DESCRIPTOR_TYPE_SAMPLER, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                          VK_DESCRIPTOR_TYPE_STORAGE_IMAGE};

    std::vector<VkDescriptorPoolSize> pool_sizes;
    for (auto type : types) {
        pool_sizes.push_back(VkDescriptorPoolSize{type, k_max_bindless_resources});
    }

    VkDescriptorPoolCreateInfo pool_create_info{};
    pool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_create_info.pNext = nullptr;
    pool_create_info.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT;

    pool_create_info.maxSets = (u32)pool_sizes.size() * k_max_bindless_resources;
    pool_create_info.poolSizeCount = static_cast<u32>(pool_sizes.size());
    pool_create_info.pPoolSizes = pool_sizes.data();

    CHECK_VK_RESULT(vkCreateDescriptorPool(m_context.device, &pool_create_info, nullptr, &m_bindless_descriptor_pool));
}

void VulkanDescriptorSetAllocator::ResetDescriptorPool() {
    for (auto &set : allocated_sets) {
        delete set;
    }
    allocated_sets.clear();
    if (m_temp_descriptor_pool)
        vkResetDescriptorPool(m_context.device, m_temp_descriptor_pool, 0);
    if (m_bindless_descriptor_pool) {
        vkResetDescriptorPool(m_context.device, m_bindless_descriptor_pool, 0);
    }
}

VkDescriptorSetLayout VulkanDescriptorSetAllocator::GetVkDescriptorSetLayout(u64 key) const {
    return m_descriptor_set_layout_map.at(key);
}

} // namespace Horizon::Backend