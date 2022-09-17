#include "VulkanDescriptorSetManager.h"

#include <algorithm>

#include <runtime/function/rhi/RHIUtils.h>
#include <runtime/function/rhi/ResourceCache.h>
#include <runtime/function/rhi/vulkan/VulkanPipeline.h>
#include <runtime/function/rhi/vulkan/VulkanShader.h>

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
    std::array<VkDescriptorSetLayoutCreateInfo, MAX_SET_COUNT_PER_PIPELINE> layout_create_infos{};

    std::array<std::vector<VkDescriptorSetLayoutBinding>, MAX_SET_COUNT_PER_PIPELINE> layout_bindings{};

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
    VulkanShader *shader, std::array<VkDescriptorSetLayoutCreateInfo, MAX_SET_COUNT_PER_PIPELINE> &layout_create_infos,
    std::array<std::vector<VkDescriptorSetLayoutBinding>, MAX_SET_COUNT_PER_PIPELINE> &layout_bindings) {
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
        auto& bindings = layout_bindings[refl_set.set];
        //bindings.resize(refl_set.binding_count);

        for (u32 binding = 0; binding < refl_set.binding_count; binding++) {
            const SpvReflectDescriptorBinding & spv_binding = *(refl_set.bindings[binding]);
            u32 index = spv_binding.binding;

            if (index >= bindings.size()) {
                bindings.resize(index + 1);
            }
            bindings[index].binding = spv_binding.binding; // binding index
            bindings[index].descriptorType =
                static_cast<VkDescriptorType>(spv_binding.descriptor_type); // descriptor type
            bindings[index].descriptorCount = 1;                    // descriptor count
            bindings[index].stageFlags |= ToVkShaderStageBit(shader->GetType());

            // descriptor pool size
            {
                auto &descriptor_count =
                    descriptor_pool_size_desc.descriptor_type_map[bindings[index].descriptorType];
                descriptor_count.required++;
                if (descriptor_count.required > descriptor_count.reserved) {
                    descriptor_pool_size_desc.recreate = true;
                    descriptor_count.reserved = descriptor_count.required * 2;
                }
            }
        }
    }
    spvReflectDestroyShaderModule(&module);
}

void VulkanDescriptorSetManager::InitEmptyDescriptorSet() {}

void VulkanDescriptorSetManager::BindResource(Pipeline *pipeline, Buffer *buffer, ResourceUpdateFrequency frequency,
                                              u32 binding) {
    auto vk_buffer = reinterpret_cast<VulkanBuffer *>(buffer);

    vk_buffer->buffer_info.buffer = vk_buffer->m_buffer;
    vk_buffer->buffer_info.offset = 0;
    vk_buffer->buffer_info.range = buffer->m_size;

    auto &info = m_pipeline_descriptors_map[pipeline].infos[static_cast<u32>(frequency)];

    if (info.set == VK_NULL_HANDLE) {
        LOG_WARN("descriptor set not allocated")
        return;
    }

    auto &write = info.writes[binding];

    // VkWriteDescriptorSet write;
    if (buffer->m_descriptor_type == DescriptorType::DESCRIPTOR_TYPE_RW_BUFFER) {
        write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    } else if (buffer->m_descriptor_type == DescriptorType::DESCRIPTOR_TYPE_CONSTANT_BUFFER) {
        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    }

    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.pNext = nullptr;
    write.dstBinding = binding;
    write.descriptorCount = 1;
    write.dstSet = info.set;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.pBufferInfo = &vk_buffer->buffer_info;
}

VulkanDescriptorSetManager::~VulkanDescriptorSetManager() noexcept {
    for (auto &layout : m_descriptor_set_layout_map) {
        vkDestroyDescriptorSetLayout(m_context.device, layout.second.layout, nullptr);
    }
    // all descriptor sets allocated from the pool are implicitly freed and become invalid
    vkDestroyDescriptorPool(m_context.device, m_descriptor_pool, nullptr);
}
void VulkanDescriptorSetManager::CreateDescriptorPool() {

    if (m_descriptor_set_layout_map.size() > descriptor_pool_size_desc.required_sets) {
        descriptor_pool_size_desc.required_sets = static_cast<u32>(m_descriptor_set_layout_map.size() * 2);
        descriptor_pool_size_desc.recreate = true;
    }
    if (!descriptor_pool_size_desc.recreate) {
        return;
    }
    std::vector<VkDescriptorPoolSize> poolSizes(descriptor_pool_size_desc.descriptor_type_map.size());

    u32 i = 0;
    for (auto &[type, count] : descriptor_pool_size_desc.descriptor_type_map) {
        poolSizes[i++] = VkDescriptorPoolSize{type, count.reserved};
    }

    if (poolSizes.empty()) {
        return;
    }
    VkDescriptorPoolCreateInfo pool_create_info{};

    pool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_create_info.pNext = nullptr;
    pool_create_info.flags = 0;
    pool_create_info.maxSets = descriptor_pool_size_desc.max_sets;
    pool_create_info.poolSizeCount = static_cast<u32>(poolSizes.size());
    pool_create_info.pPoolSizes = poolSizes.data();
    pool_create_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    CHECK_VK_RESULT(vkCreateDescriptorPool(m_context.device, &pool_create_info, nullptr, &m_descriptor_pool));
    descriptor_pool_size_desc.recreate = false;
}

void VulkanDescriptorSetManager::ResetDescriptorPool() {
    // vkResetDescriptorPool(m_device, m_bindless_descriptor_pool, 0);
}
void VulkanDescriptorSetManager::UpdatePipelineDescriptorSet(Pipeline *pipeline, ResourceUpdateFrequency frequency) {

    auto vk_pipeline = reinterpret_cast<VulkanPipeline *>(pipeline);

    auto &info = m_pipeline_descriptors_map.at(pipeline).infos[static_cast<u32>(frequency)];
    if (info.set == VK_NULL_HANDLE) {
        return;
    }

    std::vector<VkWriteDescriptorSet> writes;
    writes.reserve(info.writes.size());
    for (auto &val : info.writes) {
        if (val.dstSet != nullptr)
            writes.emplace_back(val);
    }

    vkUpdateDescriptorSets(m_context.device, static_cast<u32>(writes.size()), writes.data(), 0, nullptr);
}

VkDescriptorSetLayout VulkanDescriptorSetManager::FindLayout(u64 key) const {
    return m_descriptor_set_layout_map.at(key).layout;
}

void VulkanDescriptorSetManager::AllocateDescriptorSets(VulkanPipeline *pipeline, ResourceUpdateFrequency frequency) {
    if (m_descriptor_pool == VK_NULL_HANDLE) {
        CreateDescriptorPool();
    }

    VkDescriptorSetLayout layout =
        FindLayout(pipeline->m_pipeline_layout_desc.descriptor_set_hash_key[static_cast<u32>(frequency)]);

    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = m_descriptor_pool;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &layout;

    auto &set = m_pipeline_descriptors_map[pipeline].infos[static_cast<u32>(frequency)].set;
    CHECK_VK_RESULT(vkAllocateDescriptorSets(m_context.device, &alloc_info, &set));
}
} // namespace Horizon::RHI