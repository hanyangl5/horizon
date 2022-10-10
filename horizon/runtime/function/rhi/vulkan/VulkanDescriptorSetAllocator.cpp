#include "VulkanDescriptorSetAllocator.h"

#include <algorithm>

#include <runtime/function/rhi/RHIUtils.h>
#include <runtime/function/rhi/ResourceCache.h>
#include <runtime/function/rhi/vulkan/VulkanPipeline.h>

namespace Horizon::RHI {

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
        bindings[freq].reserve(rsd.descriptors[freq].size());
        for (auto &[name, descriptor] : rsd.descriptors[freq]) {
            VkDescriptorSetLayoutBinding binding{};
            binding.binding = descriptor.vk_binding;
            binding.descriptorCount = 1;
            binding.stageFlags = VK_SHADER_STAGE_ALL;
            binding.descriptorType = util_to_vk_descriptor_type(descriptor.type);
            bindings[freq].push_back(binding);

            // explicit descriptor count for each pool
            // pipeline_descriptor_set_resources[pipeline]
            //    .descriptor_pool_size_descs[freq]
            //    .required_descriptor_count_per_type[binding.descriptorType]++;
            // LOG_INFO("size: {}", sizeof(PipelineDescriptorSetResource));
        }
    }

    std::array<VkDescriptorSetLayoutCreateInfo, DESCRIPTOR_SET_UPDATE_FREQUENCIES> layout_create_infos{};

    // create layouts
    for (u32 i = 0; i < bindings.size(); i++) {
        // empty descriptor set
        if (bindings[i].empty()) {
            pipeline->m_pipeline_layout_desc.descriptor_set_hash_key[i] = m_empty_descriptor_set_layout_hash_key;
        } else {
            layout_create_infos[i].sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            layout_create_infos[i].bindingCount = static_cast<u32>(bindings[i].size());
            layout_create_infos[i].pBindings = bindings[i].data();
            u64 key = std::hash<VkDescriptorSetLayoutCreateInfo>{}(layout_create_infos[i]);

            auto res = m_descriptor_set_layout_map.find(key);

            if (res == m_descriptor_set_layout_map.end()) {
                VkDescriptorSetLayout layout;
                CHECK_VK_RESULT(
                    vkCreateDescriptorSetLayout(m_context.device, &layout_create_infos[i], nullptr, &layout));
                m_descriptor_set_layout_map.emplace(key, layout);
            } else {
                LOG_DEBUG("layout exist");
            }
            pipeline->m_pipeline_layout_desc.descriptor_set_hash_key[i] = key;
        }
    }
}

VulkanDescriptorSetAllocator::~VulkanDescriptorSetAllocator() noexcept {

    for (auto &layout : m_descriptor_set_layout_map) {
        vkDestroyDescriptorSetLayout(m_context.device, layout.second, nullptr);
    }

    // all descriptor sets allocated from the pool are implicitly freed and become invalid

    // for (auto &pool : m_descriptor_pools) {
    //     /*pool.clear();*/
    //     for (auto p : pool) {
    //         vkDestroyDescriptorPool(m_context.device, p, nullptr);
    //     }
    // }

    vkDestroyDescriptorPool(m_context.device, m_temp_descriptor_pool, nullptr);
}


void VulkanDescriptorSetAllocator::CreateDescriptorPool() {
    std::array<VkDescriptorType, 5> types{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                          VK_DESCRIPTOR_TYPE_SAMPLER, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                          VK_DESCRIPTOR_TYPE_STORAGE_IMAGE};
    std::vector<VkDescriptorPoolSize> pool_sizes{};
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

    // for (u32 freq = 0; freq < DESCRIPTOR_SET_UPDATE_FREQUENCIES; freq++) {
    //     if (m_descriptor_pools[freq].empty()) {
    //         std::vector<VkDescriptorPoolSize> poolSizes(
    //             descriptor_pool_size_descs[freq].required_descriptor_count_per_type.size());

    //        u32 i = 0;
    //        for (auto &[type, count] : descriptor_pool_size_descs[freq].required_descriptor_count_per_type) {
    //            poolSizes[i++] = VkDescriptorPoolSize{type, count * m_reserved_max_sets[freq]};
    //        }

    //        if (poolSizes.empty()) {
    //            continue;
    //        }

    //        VkDescriptorPoolCreateInfo pool_create_info{};
    //        pool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    //        pool_create_info.pNext = nullptr;
    //        pool_create_info.flags = 0;
    //
    //        pool_create_info.maxSets = m_reserved_max_sets[freq] * pipeline_descriptor_set_resources.size();
    //        pool_create_info.poolSizeCount = static_cast<u32>(poolSizes.size());
    //        pool_create_info.pPoolSizes = poolSizes.data();

    //        VkDescriptorPool ds_pool{};
    //        CHECK_VK_RESULT(vkCreateDescriptorPool(m_context.device, &pool_create_info, nullptr, &ds_pool));
    //        m_descriptor_pools[freq].push_back(ds_pool);
    //    }
    //}
}

void VulkanDescriptorSetAllocator::ResetDescriptorPool() {

    for (auto &set : allocated_sets) {
        delete set;
    }
    for (u32 freq = 0; freq < DESCRIPTOR_SET_UPDATE_FREQUENCIES; freq++) {

        for (auto &[pipeline, resource] : pipeline_descriptor_set_resources) {
            //resource.m_used_set_counter[freq] = 0;
        }
        if (!m_descriptor_pools.empty()) {
            // reset all descriptorpool and free all descriptors
            for (auto &pool : m_descriptor_pools) {
                vkResetDescriptorPool(m_context.device, pool, 0);
            }
        }
    }
}
VkDescriptorSetLayout VulkanDescriptorSetAllocator::GetVkDescriptorSetLayout(u64 key) const {
    return m_descriptor_set_layout_map.at(key);
}

} // namespace Horizon::RHI