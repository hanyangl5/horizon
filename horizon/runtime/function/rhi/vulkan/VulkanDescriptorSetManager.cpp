#include "VulkanDescriptorSetManager.h"

#include <algorithm>

#include <runtime/function/rhi/RHIUtils.h>
#include <runtime/function/rhi/ResourceCache.h>
#include <runtime/function/rhi/vulkan/VulkanPipeline.h>

namespace Horizon::RHI {

VulkanDescriptorSetManager::VulkanDescriptorSetManager(const VulkanRendererContext &context) noexcept
    : m_context(context) {}

PipelineLayoutDesc
VulkanDescriptorSetManager::CreateDescriptorSetLayoutFromShader(std::unordered_map<ShaderType, Shader *> &shader_map,
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
        ReflectDescriptorSetLayoutFromShader(reinterpret_cast<VulkanShader *>(shader), layout_create_infos,
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
void VulkanDescriptorSetManager::ReflectDescriptorSetLayoutFromShader(
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
            { descriptor_pool_size_descs[refl_set.set].required_descriptor_count_per_type[bindings[index].descriptorType]++; }
        }
    }
    spvReflectDestroyShaderModule(&module);
}


VulkanDescriptorSetManager::~VulkanDescriptorSetManager() noexcept {
    for (auto &layout : m_descriptor_set_layout_map) {
        vkDestroyDescriptorSetLayout(m_context.device, layout.second.layout, nullptr);
    }
    // all descriptor sets allocated from the pool are implicitly freed and become invalid
    for (auto &pool : m_descriptor_pools) {

        vkDestroyDescriptorPool(m_context.device, pool, nullptr);
    }
}
void VulkanDescriptorSetManager::CreateDescriptorPool(const std::array<u64, DESCRIPTOR_SET_UPDATE_FREQUENCIES>& descriptor_set_hash_key) {

    for (u32 freq = 0; freq < DESCRIPTOR_SET_UPDATE_FREQUENCIES; freq++) {
        std::vector<VkDescriptorPoolSize> poolSizes(descriptor_pool_size_descs[freq].required_descriptor_count_per_type.size());

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
        pool_create_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

        pool_create_info.maxSets = m_reserved_max_sets[freq];
        pool_create_info.poolSizeCount = static_cast<u32>(poolSizes.size());
        pool_create_info.pPoolSizes = poolSizes.data();

        CHECK_VK_RESULT(
            vkCreateDescriptorPool(m_context.device, &pool_create_info, nullptr, &m_descriptor_pools[freq]));

        // allocate sets to be used
        VkDescriptorSetAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;

        VkDescriptorSetLayout layout = FindLayout(descriptor_set_hash_key[freq]);
        std::vector<VkDescriptorSetLayout> layouts(m_reserved_max_sets[freq], layout);
        allocated_sets[freq].sets.resize(m_reserved_max_sets[freq]);

        alloc_info.descriptorPool = m_descriptor_pools[static_cast<u32>(freq)];
        alloc_info.descriptorSetCount = m_reserved_max_sets[freq];
        alloc_info.pSetLayouts = layouts.data();

        CHECK_VK_RESULT(vkAllocateDescriptorSets(m_context.device, &alloc_info, allocated_sets[freq].sets.data()));
    }
}

void VulkanDescriptorSetManager::ResetDescriptorPool() {
    for (u32 freq = 0; freq < DESCRIPTOR_SET_UPDATE_FREQUENCIES; freq++) {
        m_used_set_counter[freq] = 0;
        if (m_descriptor_pools[freq] != VK_NULL_HANDLE) {
            // free all descriptors
            vkResetDescriptorPool(m_context.device, m_descriptor_pools[freq], 0);
        }
    }

}

VkDescriptorSetLayout VulkanDescriptorSetManager::FindLayout(u64 key) const {
    return m_descriptor_set_layout_map.at(key).layout;
}

} // namespace Horizon::RHI