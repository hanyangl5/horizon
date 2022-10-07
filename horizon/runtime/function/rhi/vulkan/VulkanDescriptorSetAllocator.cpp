#include "VulkanDescriptorSetAllocator.h"

#include <algorithm>

#include <runtime/function/rhi/RHIUtils.h>
#include <runtime/function/rhi/ResourceCache.h>
#include <runtime/function/rhi/vulkan/VulkanPipeline.h>

namespace Horizon::RHI {

VulkanDescriptorSetAllocator::VulkanDescriptorSetAllocator(const VulkanRendererContext &context) noexcept
    : m_context(context) {}

PipelineLayoutDesc
VulkanDescriptorSetAllocator::CreateDescriptorSetLayoutFromShader(std::unordered_map<ShaderType, Shader *> &shader_map,
                                                                  PipelineType pipeline_type) {

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
        m_descriptor_set_layout_map.emplace(m_empty_descriptor_set_layout_hash_key, DescriptorSetLayout{layout});
    }

    PipelineLayoutDesc layout_desc;
    std::array<VkDescriptorSetLayoutCreateInfo, DESCRIPTOR_SET_UPDATE_FREQUENCIES> layout_create_infos{};

    std::array<std::array<VkDescriptorSetLayoutBinding, MAX_BINDING_PER_DESCRIPTOR_SET>,
               DESCRIPTOR_SET_UPDATE_FREQUENCIES>
        layout_bindings{};

    for (auto &[type, shader] : shader_map) {
        QueryDescriptorSetLayoutFromShader(reinterpret_cast<VulkanShader *>(shader), layout_create_infos,
                                             layout_bindings);
    }

    // fill empty bindings
    {
        for (auto &e : layout_bindings) {
            for (u32 i = 0; i < e.size(); i++) {
                if (e[i].descriptorCount == 0) {
                    e[i].binding = i;
                }
            }
        }
    }
    // crete layouts
    for (u32 i = 0; i < layout_create_infos.size(); i++) {
        if (layout_create_infos[i].sType == VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO) {

            layout_create_infos[i].bindingCount = static_cast<u32>(layout_bindings[i].size());
            layout_create_infos[i].pBindings = layout_bindings[i].data();
            u64 hash_key = std::hash<VkDescriptorSetLayoutCreateInfo>{}(layout_create_infos[i]);

            auto res = m_descriptor_set_layout_map.find(hash_key);

            if (res == m_descriptor_set_layout_map.end()) {
                VkDescriptorSetLayout layout;
                CHECK_VK_RESULT(
                    vkCreateDescriptorSetLayout(m_context.device, &layout_create_infos[i], nullptr, &layout));
                m_descriptor_set_layout_map.emplace(hash_key, DescriptorSetLayout{layout});
            } else {
                LOG_INFO("descriptorset exist");
            }
            layout_desc.descriptor_set_hash_key[i] = hash_key;
        } else {
            // fill empty layout
            layout_desc.descriptor_set_hash_key[i] = m_empty_descriptor_set_layout_hash_key;
        }
    }

    return layout_desc;
}

// TODO: use spirv cross instead of sprirv reflect
void VulkanDescriptorSetAllocator::QueryDescriptorSetLayoutFromShader(
    VulkanShader *shader,
    std::array<VkDescriptorSetLayoutCreateInfo, DESCRIPTOR_SET_UPDATE_FREQUENCIES> &layout_create_infos,
    std::array<std::array<VkDescriptorSetLayoutBinding, MAX_BINDING_PER_DESCRIPTOR_SET>,
               DESCRIPTOR_SET_UPDATE_FREQUENCIES> &layout_bindings) {
    SpvReflectShaderModule module;
    SpvReflectResult result =
        spvReflectCreateShaderModule(shader->m_spirv_code.size(), shader->m_spirv_code.data(), &module);
    assert(result == SPV_REFLECT_RESULT_SUCCESS);

    uint32_t count = 0;
    result = spvReflectEnumerateDescriptorSets(&module, &count, NULL);
    assert(result == SPV_REFLECT_RESULT_SUCCESS);

    std::vector<SpvReflectDescriptorSet *> sets(count);
    result = spvReflectEnumerateDescriptorSets(&module, &count, sets.data());
    assert(result == SPV_REFLECT_RESULT_SUCCESS);
    for (u32 i = 0; i < sets.size(); i++) {
        const auto &refl_set = *(sets[i]);
        auto &layout_create_info = layout_create_infos[refl_set.set];
        layout_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        auto &bindings = layout_bindings[refl_set.set];
        // bindings.resize(refl_set.binding_count);

        for (u32 binding = 0; binding < refl_set.binding_count; binding++) {
            const SpvReflectDescriptorBinding &spv_binding = *(refl_set.bindings[binding]);
            u32 index = spv_binding.binding;
            bindings[index].binding = spv_binding.binding; // binding index
            bindings[index].descriptorType =
                static_cast<VkDescriptorType>(spv_binding.descriptor_type); // descriptor type
            bindings[index].descriptorCount = 1;                            // descriptor count
            bindings[index].stageFlags |= ToVkShaderStageBit(shader->GetType());

            // descriptor pool size
            {
                u32 frequency = refl_set.set;
                descriptor_pool_size_descs[frequency]
                    .required_descriptor_count_per_type[bindings[index].descriptorType]++;
            }
        }
    }
    spvReflectDestroyShaderModule(&module);
}

VulkanDescriptorSetAllocator::~VulkanDescriptorSetAllocator() noexcept {
    for (auto &layout : m_descriptor_set_layout_map) {
        vkDestroyDescriptorSetLayout(m_context.device, layout.second.layout, nullptr);
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
    return m_descriptor_set_layout_map.at(key).layout;
}

} // namespace Horizon::RHI