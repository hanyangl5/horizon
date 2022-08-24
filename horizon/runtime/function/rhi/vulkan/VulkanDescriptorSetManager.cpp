#include <algorithm>

#include <runtime/function/rhi/ResourceCache.h>
#include <runtime/function/rhi/vulkan/VulkanDescriptorSetManager.h>
#include <runtime/function/rhi/vulkan/VulkanShaderProgram.h>

namespace Horizon::RHI {


VulkanDescriptorSetManager::VulkanDescriptorSetManager(const VulkanRendererContext &context) noexcept
    : m_context(context) {}

    // std::vector<SpvReflectDescriptorSet *>
// VulkanDescriptorSetManager::ReflectDescriptorSetLayout(void *spirv, u32 size)
// {
//     // reflection data
//     // Generate reflection data for a shader
//     SpvReflectShaderModule module;
//     SpvReflectResult result =
//         spvReflectCreateShaderModule(size, spirv, &module);
//     assert(result == SPV_REFLECT_RESULT_SUCCESS);
//
//     // Enumerate and extract shader's input variables
//     // uint32_t var_count = 0;
//     // result = spvReflectEnumerateInputVariables(&module, &var_count, NULL);
//     // assert(result == SPV_REFLECT_RESULT_SUCCESS);
//
//     uint32_t count = 0;
//     result = spvReflectEnumerateDescriptorSets(&module, &count, NULL);
//     assert(result == SPV_REFLECT_RESULT_SUCCESS);
//
//     std::vector<SpvReflectDescriptorSet *> sets(count);
//     result = spvReflectEnumerateDescriptorSets(&module, &count, sets.data());
//     assert(result == SPV_REFLECT_RESULT_SUCCESS);
//     return std::move(sets);
// }

PipelineLayoutDesc
VulkanDescriptorSetManager::CreateLayouts(std::unordered_map<ShaderType, ShaderProgram *> &shader_map,
                                          PipelineType pipeline_type) {

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
    layout_desc.descriptor_set_hash_key.resize(10);
    layout_desc.set_index.resize(10);

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
    SpvReflectResult result =
        spvReflectCreateShaderModule(cs->m_spirv_code.size(),
                                                           cs->m_spirv_code.data(), &module);
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
    layout_desc.descriptor_set_hash_key.resize(sets.size());
    layout_desc.set_index.resize(sets.size());

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

        layout_desc.set_index[i] = refl_set.set;
        layout_desc.descriptor_set_hash_key[i] = hash_key;
    }
    return layout_desc;
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
void VulkanDescriptorSetManager::Update(UpdateFrequency frequency) {

    switch (frequency) {
    case Horizon::UpdateFrequency::NONE:
        break;
    case Horizon::UpdateFrequency::PER_FRAME:
        break;
    case Horizon::UpdateFrequency::PER_BATCH:
        break;
    case Horizon::UpdateFrequency::PER_DRAW:
        break;
    default:
        break;
    }
    // TODO: create empty descriptors if no resource set

    vkUpdateDescriptorSets(m_context.device, static_cast<u32>(descriptor_writes.size()), descriptor_writes.data(), 0,
                           nullptr);
}

VkDescriptorSetLayout VulkanDescriptorSetManager::FindLayout(u64 key) const {
    return m_descriptor_set_layout_map.at(key).layout;
}

std::vector<VkDescriptorSet> VulkanDescriptorSetManager::AllocateDescriptorSets(const PipelineLayoutDesc &layout_desc) {

    std::vector<VkDescriptorSetLayout> layouts(layout_desc.descriptor_set_hash_key.size());

    for (u32 i = 0; i < layout_desc.descriptor_set_hash_key.size(); i++) {
        const auto &descriptor_set_value = m_descriptor_set_layout_map.at(layout_desc.descriptor_set_hash_key[i]);

        if (!descriptor_set_value.layout) {
            LOG_ERROR("descriptor set layout not found"); //
        }
        layouts[i] = descriptor_set_value.layout;
    }

    CreateDescriptorPool();
    if (!m_descriptor_pool) {
        return {};
    }
    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = m_descriptor_pool;
    alloc_info.descriptorSetCount = static_cast<u32>(layouts.size());
    alloc_info.pSetLayouts = layouts.data();

    std::vector<VkDescriptorSet> sets(layout_desc.descriptor_set_hash_key.size());
    CHECK_VK_RESULT(vkAllocateDescriptorSets(m_context.device, &alloc_info, sets.data()));

    return sets;
}
} // namespace Horizon::RHI