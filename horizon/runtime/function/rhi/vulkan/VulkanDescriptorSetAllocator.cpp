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

PipelineLayoutDesc
VulkanDescriptorSetAllocator::CreateDescriptorSetLayoutFromShader(std::vector<Shader *> &shaders,
                                                                  PipelineType pipeline_type) {
    PipelineLayoutDesc layout_desc{};

    std::array<std::vector<VkDescriptorSetLayoutBinding>, DESCRIPTOR_SET_UPDATE_FREQUENCIES> bindings{};

    for (auto &shader : shaders) {
        QueryDescriptorSetLayoutFromShader(reinterpret_cast<VulkanShader *>(shader), bindings);
    }

    // fill empty bindings
    {
        for (auto &e : bindings) {
            for (u32 i = 0; i < e.size(); i++) {
                if (e[i].descriptorCount == 0) {
                    e[i].binding = i;
                }
            }
        }
    }

    std::array<VkDescriptorSetLayoutCreateInfo, DESCRIPTOR_SET_UPDATE_FREQUENCIES> layout_create_infos{};

    // create layouts
    for (u32 i = 0; i < bindings.size(); i++) {
        // empty descriptor set
        if (bindings[i].empty()) {
            layout_desc.descriptor_set_hash_key[i] = m_empty_descriptor_set_layout_hash_key;
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
            layout_desc.descriptor_set_hash_key[i] = key;
        }
    }

    return layout_desc;
}

void VulkanDescriptorSetAllocator::QueryDescriptorSetLayoutFromShader(
    VulkanShader *shader,
    std::array<std::vector<VkDescriptorSetLayoutBinding>, DESCRIPTOR_SET_UPDATE_FREQUENCIES> &bindings) {

    // resize vector
    auto &vk_max_binding_index = shader->GetVkMaxBindingIndex();
    for (u32 i = 0; i < bindings.size(); i++) {
        bindings[i].resize(vk_max_binding_index[i]);
    }

    // sort shader descriptors
    auto &rsd = shader->GetRootSignatureDesc();

    for (auto &descriptor : rsd.descs) {
        auto &binding = bindings[static_cast<u32>(descriptor.update_frequency)];
        binding[descriptor.vk_binding].binding = descriptor.vk_binding;
        binding[descriptor.vk_binding].descriptorCount = 1;
        binding[descriptor.vk_binding].descriptorType = util_to_vk_descriptor_type(descriptor.type);
        binding[descriptor.vk_binding].pImmutableSamplers = nullptr;
        binding[descriptor.vk_binding].stageFlags = VK_SHADER_STAGE_ALL;

        descriptor_pool_size_descs[static_cast<u32>(descriptor.update_frequency)]
            .required_descriptor_count_per_type[binding[descriptor.vk_binding].descriptorType]++;
    }
}

VulkanDescriptorSetAllocator::~VulkanDescriptorSetAllocator() noexcept {
    for (auto &layout : m_descriptor_set_layout_map) {
        vkDestroyDescriptorSetLayout(m_context.device, layout.second, nullptr);
    }

    // all descriptor sets allocated from the pool are implicitly freed and become invalid

    for (auto &pool : m_descriptor_pools) {
        /*pool.clear();*/
        for (auto p : pool) {
            vkDestroyDescriptorPool(m_context.device, p, nullptr);
        }
    }
}
void VulkanDescriptorSetAllocator::UpdateDescriptorPoolInfo(
    Pipeline *pipeline, const std::array<u64, DESCRIPTOR_SET_UPDATE_FREQUENCIES> &descriptor_set_hash_key) {

    pipeline_descriptor_set_resources[pipeline].layout_hash_key = descriptor_set_hash_key;
}

void VulkanDescriptorSetAllocator::CreateDescriptorPool() {

    for (u32 freq = 0; freq < DESCRIPTOR_SET_UPDATE_FREQUENCIES; freq++) {
        if (m_descriptor_pools[freq].empty()) {
            std::vector<VkDescriptorPoolSize> poolSizes(
                descriptor_pool_size_descs[freq].required_descriptor_count_per_type.size());

            u32 i = 0;
            for (auto &[type, count] : descriptor_pool_size_descs[freq].required_descriptor_count_per_type) {
                poolSizes[i++] = VkDescriptorPoolSize{type, count * m_reserved_max_sets[freq]};
            }

            if (poolSizes.empty()) {
                continue;
            }

            VkDescriptorPoolCreateInfo pool_create_info{};
            pool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            pool_create_info.pNext = nullptr;
            pool_create_info.flags = 0;

            pool_create_info.maxSets = m_reserved_max_sets[freq] * pipeline_descriptor_set_resources.size();
            pool_create_info.poolSizeCount = static_cast<u32>(poolSizes.size());
            pool_create_info.pPoolSizes = poolSizes.data();

            VkDescriptorPool ds_pool{};
            CHECK_VK_RESULT(vkCreateDescriptorPool(m_context.device, &pool_create_info, nullptr, &ds_pool));
            m_descriptor_pools[freq].push_back(ds_pool);
        }
    }

    for (auto &[pipeline, resource] : pipeline_descriptor_set_resources) {
        for (u32 freq = 0; freq < DESCRIPTOR_SET_UPDATE_FREQUENCIES; freq++) {
            if (m_descriptor_pools[freq].empty()) {
                continue;
            }
            // allocate sets to be used
            VkDescriptorSetAllocateInfo alloc_info{};
            alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;

            VkDescriptorSetLayout layout = FindLayout(resource.layout_hash_key[freq]);

            if (resource.layout_hash_key[freq] == m_empty_descriptor_set_layout_hash_key) {
                continue;
            }
            std::vector<VkDescriptorSetLayout> layouts(m_reserved_max_sets[freq], layout);
            resource.allocated_sets[freq].resize(m_reserved_max_sets[freq]);

            alloc_info.descriptorPool = m_descriptor_pools[static_cast<u32>(freq)].back();
            alloc_info.descriptorSetCount = m_reserved_max_sets[freq];
            alloc_info.pSetLayouts = layouts.data();

            CHECK_VK_RESULT(
                vkAllocateDescriptorSets(m_context.device, &alloc_info, resource.allocated_sets[freq].data()));
        }
    }
}

void VulkanDescriptorSetAllocator::ResetDescriptorPool() {
    for (u32 freq = 0; freq < DESCRIPTOR_SET_UPDATE_FREQUENCIES; freq++) {

        for (auto &[pipeline, resource] : pipeline_descriptor_set_resources) {
            resource.m_used_set_counter[freq] = 0;
        }
        if (!m_descriptor_pools[freq].empty()) {
            // reset all descriptorpool and free all descriptors
            for (auto &pool : m_descriptor_pools[freq]) {
                vkResetDescriptorPool(m_context.device, pool, 0);
            }
        }
    }
    pool_created = false;
}
VkDescriptorSetLayout VulkanDescriptorSetAllocator::FindLayout(u64 key) const {
    return m_descriptor_set_layout_map.at(key);
}

} // namespace Horizon::RHI