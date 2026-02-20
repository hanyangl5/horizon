#include "vulkan_descriptor_set_allocator.h"

#include <algorithm>

#define SPIRV_REFLECT_USE_SYSTEM_SPIRV_H
#include <spirv_reflect.h>

#include <rhi/vulkan/vulkan_pipeline.h>
#include <rhi/vulkan/vulkan_resource_cache.h>

namespace Horizon::Backend
{

VulkanDescriptorSetAllocator::VulkanDescriptorSetAllocator(const VulkanRendererContext &context) noexcept
    : m_context(context)
{
    // create empty layout
    if (m_empty_descriptor_set == VK_NULL_HANDLE || m_empty_descriptor_set_layout_hash_key == 0)
    {

        VkDescriptorSetLayoutCreateInfo set_layout_create_info{};
        set_layout_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;

        set_layout_create_info.flags = 0;
        // set_layout_create_info.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
        set_layout_create_info.pNext = nullptr;
        set_layout_create_info.bindingCount = 0;
        set_layout_create_info.pBindings = nullptr;

        VkDescriptorSetLayout layout;
        CHECK_VK_RESULT(vkCreateDescriptorSetLayout(m_context.device, &set_layout_create_info, nullptr, &layout));

        m_empty_descriptor_set_layout_hash_key = std::hash<VkDescriptorSetLayoutCreateInfo>{}(set_layout_create_info);
        m_descriptor_set_layout_map.emplace(m_empty_descriptor_set_layout_hash_key, layout);
    }
}

void VulkanDescriptorSetAllocator::CreateDescriptorSetLayout(VulkanPipeline *pipeline)
{
    auto &rsd = pipeline->GetRootSignatureDesc();

    // Process each descriptor set from shader1
    for (const auto &[set_number, descriptors] : rsd.descriptors)
    {
        // Check if this is a bindless set (typically set 4, but can be any set with bindless resources)
        // For now, we'll detect bindless by checking if any descriptor has a large array size
        // A better approach would be to use shader metadata or conventions
        bool is_bindless = false;
        // If we have bindless resources, they're typically in a specific set
        // For now, we'll use a heuristic: if set == 1, treat as bindless
        // This can be made configurable or determined from shader metadata
        if (set_number == BINDLESS_DESCRIPTOR_SET_NUMBER)
        {
            is_bindless = true;
        }

        if (is_bindless)
        {
            // Create bindless layout
            std::vector<VkDescriptorSetLayoutBinding> bindings;
            bindings.reserve(descriptors.size());

            for (const auto &[name, descriptor] : descriptors)
            {
                VkDescriptorSetLayoutBinding binding{};
                binding.binding = descriptor.vk_binding;
                binding.descriptorCount = k_max_bindless_resources;
                binding.stageFlags = VK_SHADER_STAGE_ALL;
                binding.descriptorType = util_to_vk_descriptor_type(descriptor.type);
                bindings.push_back(binding);
            }

            // Binding flags for bindless
            VkDescriptorSetLayoutBindingFlagsCreateInfoEXT set_layout_binding_flags{};
            set_layout_binding_flags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT;
            set_layout_binding_flags.bindingCount = static_cast<u32>(bindings.size());
            std::vector<VkDescriptorBindingFlags> flags(bindings.size(),
                                                        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT |
                                                            VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT |
                                                            VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT);
            set_layout_binding_flags.pBindingFlags = flags.data();

            VkDescriptorSetLayoutCreateInfo layout_create_info{};
            layout_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            layout_create_info.bindingCount = static_cast<u32>(bindings.size());
            layout_create_info.pBindings = bindings.data();
            layout_create_info.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT;
            layout_create_info.pNext = &set_layout_binding_flags;

            u64 key = std::hash<VkDescriptorSetLayoutCreateInfo>{}(layout_create_info);
            auto res = m_descriptor_set_layout_map.find(key);

            if (res == m_descriptor_set_layout_map.end())
            {
                VkDescriptorSetLayout layout;
                CHECK_VK_RESULT(vkCreateDescriptorSetLayout(m_context.device, &layout_create_info, nullptr, &layout));
                m_descriptor_set_layout_map.emplace(key, layout);
            }
            pipeline->m_pipeline_layout_desc.bindless_descriptor_set_hash_key = key;
        }
        else
        {
            // Create regular layout
            std::vector<VkDescriptorSetLayoutBinding> bindings;
            bindings.reserve(descriptors.size());

            for (const auto &[name, descriptor] : descriptors)
            {
                VkDescriptorSetLayoutBinding binding{};
                binding.binding = descriptor.vk_binding;
                binding.descriptorCount = 1;
                binding.stageFlags = VK_SHADER_STAGE_ALL;
                binding.descriptorType = util_to_vk_descriptor_type(descriptor.type);
                bindings.push_back(binding);
            }

            if (bindings.empty())
            {
                pipeline->m_pipeline_layout_desc.descriptor_set_hash_key = m_empty_descriptor_set_layout_hash_key;
            }
            else
            {
                VkDescriptorSetLayoutCreateInfo layout_create_info{};
                layout_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
                layout_create_info.bindingCount = static_cast<u32>(bindings.size());
                layout_create_info.pBindings = bindings.data();

                u64 key = std::hash<VkDescriptorSetLayoutCreateInfo>{}(layout_create_info);
                auto res = m_descriptor_set_layout_map.find(key);

                if (res == m_descriptor_set_layout_map.end())
                {
                    VkDescriptorSetLayout layout;
                    CHECK_VK_RESULT(
                        vkCreateDescriptorSetLayout(m_context.device, &layout_create_info, nullptr, &layout));
                    m_descriptor_set_layout_map.emplace(key, layout);
                }
                pipeline->m_pipeline_layout_desc.descriptor_set_hash_key = key;
            }
        }
    }
}

VulkanDescriptorSet *VulkanDescriptorSetAllocator::GetDescriptorSet(VulkanPipeline *pipeline)
{
    if (pipeline->m_pipeline_layout_desc.descriptor_set_hash_key == 0)
    {
        return nullptr;
    }
    if (!m_temp_descriptor_pool)
    {
        CreateDescriptorPool();
    }
    if (allocated_descriptorsets[pipeline])
    {
        return allocated_descriptorsets[pipeline];
    }
    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;

    VkDescriptorSetLayout layout = GetVkDescriptorSetLayout(pipeline->m_pipeline_layout_desc.descriptor_set_hash_key);

    alloc_info.descriptorPool = m_temp_descriptor_pool;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &layout;
    VkDescriptorSet vk_ds{};
    CHECK_VK_RESULT(vkAllocateDescriptorSets(m_context.device, &alloc_info, &vk_ds));
    const auto &write_descs = pipeline->GetRootSignatureDesc().descriptors.at(DEFAULT_DESCRIPTOR_SET_NUMBER);

    VulkanDescriptorSet *set = new VulkanDescriptorSet(m_context, DEFAULT_DESCRIPTOR_SET_NUMBER, write_descs, vk_ds);
    allocated_descriptorsets[pipeline] = set;
    return set;
}

VulkanDescriptorSet *VulkanDescriptorSetAllocator::GetBindlessDescriptorSet(VulkanPipeline *pipeline)
{
    if (pipeline->m_pipeline_layout_desc.bindless_descriptor_set_hash_key == 0)
    {
        return nullptr;
    }
    if (!m_bindless_descriptor_pool)
    {
        CreateBindlessDescriptorPool();
    }
    if (allocated_bindless_descriptorsets[pipeline])
    {
        return allocated_bindless_descriptorsets[pipeline];
    }
    VkDescriptorSetVariableDescriptorCountAllocateInfo variableDescriptorCountInfo = {};
    variableDescriptorCountInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
    variableDescriptorCountInfo.descriptorSetCount = 1; // Number of descriptor sets being allocated
    u32 maxDescriptors = k_max_bindless_resources;      // Maximum descriptors for the variable-sized binding
    variableDescriptorCountInfo.pDescriptorCounts = &maxDescriptors;

    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;

    VkDescriptorSetLayout layout =
        GetVkDescriptorSetLayout(pipeline->m_pipeline_layout_desc.bindless_descriptor_set_hash_key);

    alloc_info.descriptorPool = m_bindless_descriptor_pool;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &layout;
    alloc_info.pNext = &variableDescriptorCountInfo;
    VkDescriptorSet vk_ds{};
    CHECK_VK_RESULT(vkAllocateDescriptorSets(m_context.device, &alloc_info, &vk_ds));
    const auto &write_descs = pipeline->GetRootSignatureDesc().descriptors.at(BINDLESS_DESCRIPTOR_SET_NUMBER);
    VulkanDescriptorSet *set = new VulkanDescriptorSet(m_context, BINDLESS_DESCRIPTOR_SET_NUMBER, write_descs, vk_ds);
    allocated_bindless_descriptorsets[pipeline] = set;
    return set;
}

VulkanDescriptorSetAllocator::~VulkanDescriptorSetAllocator() noexcept
{
    for (auto &layout : m_descriptor_set_layout_map)
    {
        vkDestroyDescriptorSetLayout(m_context.device, layout.second, nullptr);
    }

    vkDestroyDescriptorPool(m_context.device, m_temp_descriptor_pool, nullptr);
    if (m_bindless_descriptor_pool != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(m_context.device, m_bindless_descriptor_pool, nullptr);
    }
}

void VulkanDescriptorSetAllocator::CreateDescriptorPool()
{
    std::array<VkDescriptorType, 5> types{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                          VK_DESCRIPTOR_TYPE_SAMPLER, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                          VK_DESCRIPTOR_TYPE_STORAGE_IMAGE};

    std::vector<VkDescriptorPoolSize> pool_sizes;
    for (auto type : types)
    {
        pool_sizes.push_back(VkDescriptorPoolSize{type, 2048});
    }

    VkDescriptorPoolCreateInfo pool_create_info{};
    pool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_create_info.pNext = nullptr;
    pool_create_info.flags = 0;
    // pool_create_info.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;

    pool_create_info.maxSets = 2048;
    pool_create_info.poolSizeCount = static_cast<u32>(pool_sizes.size());
    pool_create_info.pPoolSizes = pool_sizes.data();

    CHECK_VK_RESULT(vkCreateDescriptorPool(m_context.device, &pool_create_info, nullptr, &m_temp_descriptor_pool));
}

void VulkanDescriptorSetAllocator::CreateBindlessDescriptorPool()
{
    std::array<VkDescriptorType, 5> types{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                          VK_DESCRIPTOR_TYPE_SAMPLER, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                          VK_DESCRIPTOR_TYPE_STORAGE_IMAGE};

    std::vector<VkDescriptorPoolSize> pool_sizes;
    for (auto type : types)
    {
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

void VulkanDescriptorSetAllocator::ResetDescriptorPool()
{
    for (auto &set : allocated_descriptorsets)
    {
        delete set.second;
    }
    allocated_descriptorsets.clear();
    for (auto &set : allocated_bindless_descriptorsets)
    {
        delete set.second;
    }
    allocated_bindless_descriptorsets.clear();
    if (m_temp_descriptor_pool)
    {
        vkResetDescriptorPool(m_context.device, m_temp_descriptor_pool, 0);
    }
    if (m_bindless_descriptor_pool)
    {
        vkResetDescriptorPool(m_context.device, m_bindless_descriptor_pool, 0);
    }
}

VkDescriptorSetLayout VulkanDescriptorSetAllocator::GetVkDescriptorSetLayout(u64 key) const
{
    return m_descriptor_set_layout_map.at(key);
}

} // namespace Horizon::Backend