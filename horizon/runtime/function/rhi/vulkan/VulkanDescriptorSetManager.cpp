#include <algorithm>

#include <runtime/function/rhi/ResourceCache.h>
#include <runtime/function/rhi/vulkan/VulkanDescriptorSetManager.h>
#include <runtime/function/rhi/vulkan/VulkanPipeline.h>
#include <runtime/function/rhi/vulkan/VulkanShaderProgram.h>

namespace Horizon::RHI {

VulkanDescriptorSetManager::VulkanDescriptorSetManager(const VulkanRendererContext &context) noexcept
    : m_context(context) {}

PipelineLayoutDesc VulkanDescriptorSetManager::CreateDescriptorSetLayoutFromShader(
    std::unordered_map<ShaderType, ShaderProgram *> &shader_map, PipelineType pipeline_type) {

    // create empty layout
    if (m_empty_descriptor_set == VK_NULL_HANDLE) {

        VkDescriptorSetLayoutCreateInfo set_layout_create_info{};
        set_layout_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;

        set_layout_create_info.flags = 0;
        set_layout_create_info.pNext = nullptr;
        set_layout_create_info.bindingCount = 0;
        set_layout_create_info.pBindings = nullptr;

        VkDescriptorSetLayout layout;
        CHECK_VK_RESULT(vkCreateDescriptorSetLayout(m_context.device, &set_layout_create_info, nullptr, &layout));

        m_empty_descriptor_set_layout_hash_key = std::hash<VkDescriptorSetLayoutCreateInfo>{}(set_layout_create_info);
        m_descriptor_set_layout_map.emplace(m_empty_descriptor_set_layout_hash_key, DescriptorSetValue{layout});
    }
    // combine vs/gs/ps to get pipeline layout
    if (pipeline_type == PipelineType::GRAPHICS) {
        return GetGraphicsPipelineLayout(reinterpret_cast<VulkanShaderProgram *>(shader_map[ShaderType::VERTEX_SHADER]),
                                         reinterpret_cast<VulkanShaderProgram *>(shader_map[ShaderType::PIXEL_SHADER]));

    } else if (pipeline_type == PipelineType::COMPUTE) {
        return GetComputePipelineLayout(
            reinterpret_cast<VulkanShaderProgram *>(shader_map[ShaderType::COMPUTE_SHADER]));

    } else if (pipeline_type == PipelineType::RAY_TRACING) {
        // TODO
    }

    return {};
}

// TBD:
PipelineLayoutDesc VulkanDescriptorSetManager::GetGraphicsPipelineLayout(VulkanShaderProgram *vs,
                                                                         VulkanShaderProgram *ps) {

    PipelineLayoutDesc layout_desc;
    // layout_desc.set_index.resize(10);

    //// vs
    //{
    //    SpvReflectShaderModule module;
    //    SpvReflectResult result = spvReflectCreateShaderModule(vs->m_shader_byte_code->GetBufferSize(),
    //                                                           vs->m_shader_byte_code->GetBufferPointer(), &module);
    //    assert(result == SPV_REFLECT_RESULT_SUCCESS);

    //    // Enumerate and extract shader's input variables, TODO: derive vertex input layout by shader?
    //    // uint32_t var_count = 0;
    //    // result = spvReflectEnumerateInputVariables(&module, &var_count, NULL);
    //    // assert(result == SPV_REFLECT_RESULT_SUCCESS);

    //    uint32_t count = 0;
    //    result = spvReflectEnumerateDescriptorSets(&module, &count, NULL);
    //    assert(result == SPV_REFLECT_RESULT_SUCCESS);

    //    std::vector<SpvReflectDescriptorSet *> sets(count);
    //    result = spvReflectEnumerateDescriptorSets(&module, &count, sets.data());
    //    assert(result == SPV_REFLECT_RESULT_SUCCESS);

    //    for (u32 i = 0; i < sets.size(); i++) {
    //        const auto &refl_set = *(sets[i]);

    //        VkDescriptorSetLayoutCreateInfo set_layout_create_info{};
    //        set_layout_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;

    //        set_layout_create_info.bindingCount = refl_set.binding_count;

    //        std::vector<VkDescriptorSetLayoutBinding> layout_bindings(refl_set.binding_count);

    //        for (u32 binding = 0; binding < refl_set.binding_count; binding++) {
    //            const SpvReflectDescriptorBinding &refl_binding = *(refl_set.bindings[binding]);
    //            layout_bindings[binding].binding = refl_binding.binding; // binding index
    //            layout_bindings[binding].descriptorType =
    //                static_cast<VkDescriptorType>(refl_binding.descriptor_type); // descriptor type
    //            layout_bindings[binding].descriptorCount = 1;                    // descriptor count

    //            // for (u32 i_dim = 0; i_dim < refl_binding.array.dims_count;
    //            //      ++i_dim) {
    //            //     layout_bindings[binding].descriptorCount *=
    //            //         refl_binding.array.dims[i_dim];
    //            // }

    //            layout_bindings[binding].stageFlags = VK_SHADER_STAGE_ALL; // cet in cs may also used in other
    //                                                                       // pipeline, any optimizaion here?
    //            auto &descriptor_count =
    //                descriptor_pool_size_desc.descriptor_type_map[layout_bindings[binding].descriptorType];
    //            descriptor_count.required++;
    //            if (descriptor_count.required > descriptor_count.reserved) {
    //                descriptor_pool_size_desc.recreate = true;
    //                descriptor_count.reserved = descriptor_count.required * 2;
    //            }
    //        }
    //        set_layout_create_info.bindingCount = static_cast<u32>(layout_bindings.size());
    //        set_layout_create_info.pBindings = layout_bindings.data();

    //        u64 hash_key = std::hash<VkDescriptorSetLayoutCreateInfo>{}(set_layout_create_info);

    //        auto res = m_descriptor_set_layout_map.find(hash_key);

    //        if (res == m_descriptor_set_layout_map.end()) {
    //            VkDescriptorSetLayout layout;
    //            vkCreateDescriptorSetLayout(m_context.device, &set_layout_create_info, nullptr, &layout);

    //            m_descriptor_set_layout_map.emplace(hash_key, DescriptorSetValue{layout});
    //        } else {
    //            LOG_INFO("descriptorset exist");
    //        }

    //        layout_desc.set_index[i] = refl_set.set;
    //        layout_desc.descriptor_set_hash_key[i] = hash_key;
    //    }
    //}

    // ps

    return layout_desc;
}

PipelineLayoutDesc VulkanDescriptorSetManager::GetComputePipelineLayout(VulkanShaderProgram *cs) {
    SpvReflectShaderModule module;
    SpvReflectResult result = spvReflectCreateShaderModule(cs->m_spirv_code.size(), cs->m_spirv_code.data(), &module);
    assert(result == SPV_REFLECT_RESULT_SUCCESS);

    // Enumerate and extract shader's input variables
    // uint32_t var_count = 0;
    // result = spvReflectEnumerateInputVariables(&module, &var_count, NULL);
    // assert(result == SPV_REFLECT_RESULT_SUCCESS);

    uint32_t count = 0;
    result = spvReflectEnumerateDescriptorSets(&module, &count, NULL);
    assert(result == SPV_REFLECT_RESULT_SUCCESS);

    std::vector<SpvReflectDescriptorSet *> sets(count);
    result = spvReflectEnumerateDescriptorSets(&module, &count, sets.data());
    assert(result == SPV_REFLECT_RESULT_SUCCESS);

    PipelineLayoutDesc layout_desc;

    for (u32 i = 0; i < sets.size(); i++) {
        const auto &refl_set = *(sets[i]);

        VkDescriptorSetLayoutCreateInfo set_layout_create_info{};
        set_layout_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;

        set_layout_create_info.bindingCount = refl_set.binding_count;

        std::vector<VkDescriptorSetLayoutBinding> layout_bindings(refl_set.binding_count);

        for (u32 binding = 0; binding < refl_set.binding_count; binding++) {
            const SpvReflectDescriptorBinding &refl_binding = *(refl_set.bindings[binding]);
            layout_bindings[binding].binding = refl_binding.binding; // binding index
            layout_bindings[binding].descriptorType =
                static_cast<VkDescriptorType>(refl_binding.descriptor_type); // descriptor type
            layout_bindings[binding].descriptorCount = 1;                    // descriptor count

            // for (u32 i_dim = 0; i_dim < refl_binding.array.dims_count;
            //      ++i_dim) {
            //     layout_bindings[binding].descriptorCount *=
            //         refl_binding.array.dims[i_dim];
            // }

            layout_bindings[binding].stageFlags = VK_SHADER_STAGE_ALL; // cet in cs may also used in other
                                                                       // pipeline, any optimizaion here?
            auto &descriptor_count =
                descriptor_pool_size_desc.descriptor_type_map[layout_bindings[binding].descriptorType];
            descriptor_count.required++;
            if (descriptor_count.required > descriptor_count.reserved) {
                descriptor_pool_size_desc.recreate = true;
                descriptor_count.reserved = descriptor_count.required * 2;
            }
        }
        set_layout_create_info.bindingCount = static_cast<u32>(layout_bindings.size());
        set_layout_create_info.pBindings = layout_bindings.data();
        u64 hash_key = std::hash<VkDescriptorSetLayoutCreateInfo>{}(set_layout_create_info);

        auto res = m_descriptor_set_layout_map.find(hash_key);

        if (res == m_descriptor_set_layout_map.end()) {
            VkDescriptorSetLayout layout;
            vkCreateDescriptorSetLayout(m_context.device, &set_layout_create_info, nullptr, &layout);
            m_descriptor_set_layout_map.emplace(hash_key, DescriptorSetValue{layout});
        } else {
            LOG_INFO("descriptorset exist");
        }

        // layout_desc.set_index[i] = ;
        layout_desc.descriptor_set_hash_key[refl_set.set] = hash_key;
    }

    for (auto &key : layout_desc.descriptor_set_hash_key) {
        if (key == 0) {
            key = m_empty_descriptor_set_layout_hash_key;
        }
    }
    return layout_desc;
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

VulkanDescriptorSetManager::~VulkanDescriptorSetManager() {
    // TODO: destroy descriptor resources, descriptorsetlayout, descriptorpool
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