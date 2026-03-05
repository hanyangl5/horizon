#include "vulkan_descriptor_set_allocator.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

#define SPIRV_REFLECT_USE_SYSTEM_SPIRV_H
#include <spirv_reflect.h>

#include <rhi/vulkan/vulkan_pipeline.h>
#include <rhi/vulkan/vulkan_resource_cache.h>

namespace Horizon::Backend
{
namespace
{
inline u64 HashCombine(u64 seed, u64 value)
{
    return seed ^ (value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2));
}

u64 BuildDescriptorSetLayoutKey(const std::vector<VkDescriptorSetLayoutBinding> &bindings,
                                VkDescriptorSetLayoutCreateFlags layout_flags,
                                const std::vector<VkDescriptorBindingFlags> *binding_flags = nullptr)
{
    u64 key = 1469598103934665603ULL;
    key = HashCombine(key, static_cast<u64>(layout_flags));
    key = HashCombine(key, static_cast<u64>(bindings.size()));

    for (size_t i = 0; i < bindings.size(); ++i)
    {
        const auto &binding = bindings[i];
        key = HashCombine(key, static_cast<u64>(binding.binding));
        key = HashCombine(key, static_cast<u64>(binding.descriptorType));
        key = HashCombine(key, static_cast<u64>(binding.descriptorCount));
        key = HashCombine(key, static_cast<u64>(binding.stageFlags));
        if (binding_flags && i < binding_flags->size())
        {
            key = HashCombine(key, static_cast<u64>((*binding_flags)[i]));
        }
    }

    return key;
}

bool IsRecoverableAllocateError(VkResult result)
{
    return result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL;
}

u32 GetResolvedDescriptorCount(const DescriptorDesc &desc)
{
    if (desc.descriptor_count == 0)
    {
        return 1;
    }
    return std::max(1u, desc.descriptor_count);
}

VkResult AllocateDescriptorSetWithOptionalVariableCount(VkDevice device, VkDescriptorPool pool, VkDescriptorSetLayout layout,
                                                        u32 variable_descriptor_count, VkDescriptorSet &out_set)
{
    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = pool;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &layout;

    VkDescriptorSetVariableDescriptorCountAllocateInfo variable_count_info{};
    if (variable_descriptor_count > 1)
    {
        variable_count_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
        variable_count_info.descriptorSetCount = 1;
        variable_count_info.pDescriptorCounts = &variable_descriptor_count;
        alloc_info.pNext = &variable_count_info;
    }

    return vkAllocateDescriptorSets(device, &alloc_info, &out_set);
}
} // namespace

VulkanDescriptorSetAllocator::VulkanDescriptorSetAllocator(const VulkanRendererContext &context) noexcept
    : m_context(context)
{
    InitializeDescriptorIndexingFeatures();
    InitializeUpdateAfterBindLimits();

    VkDescriptorSetLayoutCreateInfo set_layout_create_info{};
    set_layout_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    set_layout_create_info.flags = 0;
    set_layout_create_info.pNext = nullptr;
    set_layout_create_info.bindingCount = 0;
    set_layout_create_info.pBindings = nullptr;

    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    CHECK_VK_RESULT(vkCreateDescriptorSetLayout(m_context.device, &set_layout_create_info, nullptr, &layout));
    m_empty_descriptor_set_layout_hash_key = BuildDescriptorSetLayoutKey({}, set_layout_create_info.flags);
    m_descriptor_set_layout_map.emplace(m_empty_descriptor_set_layout_hash_key, layout);
}

void VulkanDescriptorSetAllocator::InitializeDescriptorIndexingFeatures()
{
    if (m_context.active_gpu == VK_NULL_HANDLE)
    {
        return;
    }

    m_descriptor_indexing_features = {};
    m_descriptor_indexing_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;

    VkPhysicalDeviceFeatures2 device_features{};
    device_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    device_features.pNext = &m_descriptor_indexing_features;
    vkGetPhysicalDeviceFeatures2(m_context.active_gpu, &device_features);
}

void VulkanDescriptorSetAllocator::InitializeUpdateAfterBindLimits()
{
    if (m_context.active_gpu == VK_NULL_HANDLE)
    {
        return;
    }

    VkPhysicalDeviceDescriptorIndexingProperties descriptor_indexing_properties{};
    descriptor_indexing_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES;

    VkPhysicalDeviceProperties2 device_properties{};
    device_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    device_properties.pNext = &descriptor_indexing_properties;
    vkGetPhysicalDeviceProperties2(m_context.active_gpu, &device_properties);

    m_max_uab_uniform_buffers = std::max(1u, descriptor_indexing_properties.maxDescriptorSetUpdateAfterBindUniformBuffers);
    m_max_uab_storage_buffers = std::max(1u, descriptor_indexing_properties.maxDescriptorSetUpdateAfterBindStorageBuffers);
    m_max_uab_samplers = std::max(1u, descriptor_indexing_properties.maxDescriptorSetUpdateAfterBindSamplers);
    m_max_uab_sampled_images = std::max(1u, descriptor_indexing_properties.maxDescriptorSetUpdateAfterBindSampledImages);
    m_max_uab_storage_images = std::max(1u, descriptor_indexing_properties.maxDescriptorSetUpdateAfterBindStorageImages);
    m_max_uab_descriptors_all_pools = descriptor_indexing_properties.maxUpdateAfterBindDescriptorsInAllPools;

    LOG_INFO("Bindless limits (UAB): UB={}, SB={}, S={}, SI={}, STI={}, ALL_POOLS={}", m_max_uab_uniform_buffers,
             m_max_uab_storage_buffers, m_max_uab_samplers, m_max_uab_sampled_images, m_max_uab_storage_images,
             m_max_uab_descriptors_all_pools);
}

u32 VulkanDescriptorSetAllocator::GetUpdateAfterBindTypeLimit(VkDescriptorType type) const
{
    switch (type)
    {
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        return m_max_uab_uniform_buffers;
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
        return m_max_uab_storage_buffers;
    case VK_DESCRIPTOR_TYPE_SAMPLER:
        return m_max_uab_samplers;
    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        return m_max_uab_sampled_images;
    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
        return m_max_uab_storage_images;
    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
        return std::max(1u, std::min(m_max_uab_samplers, m_max_uab_sampled_images));
    default:
        return 1;
    }
}

bool VulkanDescriptorSetAllocator::CheckBindlessFeatureSupport(const std::vector<VkDescriptorSetLayoutBinding> &bindings,
                                                               bool requires_variable_descriptor) const
{
    if (!m_descriptor_indexing_features.runtimeDescriptorArray ||
        !m_descriptor_indexing_features.descriptorBindingPartiallyBound)
    {
        LOG_ERROR("Bindless layout requested but descriptor indexing features runtimeDescriptorArray/partiallyBound are "
                  "not supported");
        return false;
    }
    if (requires_variable_descriptor && !m_descriptor_indexing_features.descriptorBindingVariableDescriptorCount)
    {
        LOG_ERROR("Bindless layout requests VARIABLE_DESCRIPTOR_COUNT but "
                  "descriptorBindingVariableDescriptorCount is unsupported");
        return false;
    }

    for (const auto &binding : bindings)
    {
        bool supported = false;
        switch (binding.descriptorType)
        {
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            supported = m_descriptor_indexing_features.descriptorBindingUniformBufferUpdateAfterBind == VK_TRUE;
            break;
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            supported = m_descriptor_indexing_features.descriptorBindingStorageBufferUpdateAfterBind == VK_TRUE;
            break;
        case VK_DESCRIPTOR_TYPE_SAMPLER:
        case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
            supported = m_descriptor_indexing_features.descriptorBindingSampledImageUpdateAfterBind == VK_TRUE;
            break;
        case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
            supported = m_descriptor_indexing_features.descriptorBindingStorageImageUpdateAfterBind == VK_TRUE;
            break;
        case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
            supported = m_descriptor_indexing_features.descriptorBindingUniformTexelBufferUpdateAfterBind == VK_TRUE;
            break;
        case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
            supported = m_descriptor_indexing_features.descriptorBindingStorageTexelBufferUpdateAfterBind == VK_TRUE;
            break;
        default:
            supported = false;
            break;
        }

        if (!supported)
        {
            LOG_ERROR("Bindless layout requests UPDATE_AFTER_BIND for unsupported descriptor type={} (binding={})",
                      static_cast<u32>(binding.descriptorType), binding.binding);
            return false;
        }
    }

    return true;
}

std::unordered_map<VkDescriptorType, u32> VulkanDescriptorSetAllocator::BuildPerSetTypeCounts(
    const std::unordered_map<std::string, DescriptorDesc> &descriptors) const
{
    std::unordered_map<VkDescriptorType, u32> type_counts;
    for (const auto &[name, descriptor] : descriptors)
    {
        (void)name;
        const VkDescriptorType type = util_to_vk_descriptor_type(descriptor.type);
        type_counts[type] += GetResolvedDescriptorCount(descriptor);
    }
    return type_counts;
}

bool VulkanDescriptorSetAllocator::CreateDefaultPool(const std::unordered_map<VkDescriptorType, u32> &per_set_type_counts,
                                                     u32 max_sets, VkDescriptorPool &out_pool) const
{
    if (per_set_type_counts.empty())
    {
        return false;
    }

    std::vector<VkDescriptorPoolSize> pool_sizes;
    pool_sizes.reserve(per_set_type_counts.size());
    for (const auto &[type, per_set_count] : per_set_type_counts)
    {
        const u64 scaled_count = static_cast<u64>(std::max(1u, per_set_count)) * static_cast<u64>(std::max(1u, max_sets));
        pool_sizes.push_back(VkDescriptorPoolSize{type, static_cast<u32>(std::min<u64>(scaled_count, UINT32_MAX))});
    }

    VkDescriptorPoolCreateInfo pool_create_info{};
    pool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_create_info.flags = 0;
    pool_create_info.maxSets = std::max(1u, max_sets);
    pool_create_info.poolSizeCount = static_cast<u32>(pool_sizes.size());
    pool_create_info.pPoolSizes = pool_sizes.data();

    const VkResult result = vkCreateDescriptorPool(m_context.device, &pool_create_info, nullptr, &out_pool);
    if (result != VK_SUCCESS)
    {
        LOG_ERROR("Failed to create default descriptor pool (maxSets={}, types={}, VkResult={})", pool_create_info.maxSets,
                  pool_create_info.poolSizeCount, static_cast<int>(result));
        return false;
    }

    return true;
}
bool VulkanDescriptorSetAllocator::EnsureBindlessPool()
{
    if (m_bindless_descriptor_pool != VK_NULL_HANDLE)
    {
        return true;
    }

    m_bindless_pool_max_sets = std::max(m_bindless_pool_max_sets, static_cast<u32>(m_bindless_layout_meta.size()));
    m_bindless_pool_max_sets = std::max(1u, m_bindless_pool_max_sets);

    struct PoolTypeLimit
    {
        VkDescriptorType type;
        u32 count;
    };

    std::array<PoolTypeLimit, 6> base_limits{{
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, m_max_uab_uniform_buffers},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, m_max_uab_storage_buffers},
        {VK_DESCRIPTOR_TYPE_SAMPLER, m_max_uab_samplers},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, m_max_uab_sampled_images},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, m_max_uab_storage_images},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, std::max(1u, std::min(m_max_uab_samplers, m_max_uab_sampled_images))},
    }};

    u64 total_descriptors = 0;
    for (const auto &entry : base_limits)
    {
        total_descriptors += entry.count;
    }

    double global_scale = 1.0;
    if (m_max_uab_descriptors_all_pools > 0 && total_descriptors > m_max_uab_descriptors_all_pools)
    {
        global_scale = static_cast<double>(m_max_uab_descriptors_all_pools) / static_cast<double>(total_descriptors);
    }

    double allocation_scale = 1.0;
    while (true)
    {
        std::vector<VkDescriptorPoolSize> pool_sizes;
        pool_sizes.reserve(base_limits.size());
        bool has_count_above_one = false;

        for (const auto &entry : base_limits)
        {
            const double scaled = std::floor(static_cast<double>(entry.count) * global_scale * allocation_scale);
            const u32 count = std::max(1u, static_cast<u32>(scaled));
            has_count_above_one = has_count_above_one || (count > 1);
            pool_sizes.push_back(VkDescriptorPoolSize{entry.type, count});
        }

        VkDescriptorPoolCreateInfo pool_create_info{};
        pool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_create_info.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT;
        pool_create_info.maxSets = std::max(1u, m_bindless_pool_max_sets);
        pool_create_info.poolSizeCount = static_cast<u32>(pool_sizes.size());
        pool_create_info.pPoolSizes = pool_sizes.data();

        const VkResult result =
            vkCreateDescriptorPool(m_context.device, &pool_create_info, nullptr, &m_bindless_descriptor_pool);
        if (result == VK_SUCCESS)
        {
            LOG_INFO("Create bindless descriptor pool: maxSets={}, scale={}, UB={}, SB={}, S={}, SI={}, STI={}, CIS={}",
                     pool_create_info.maxSets, global_scale * allocation_scale, pool_sizes[0].descriptorCount,
                     pool_sizes[1].descriptorCount, pool_sizes[2].descriptorCount, pool_sizes[3].descriptorCount,
                     pool_sizes[4].descriptorCount, pool_sizes[5].descriptorCount);
            return true;
        }

        if ((result == VK_ERROR_OUT_OF_DEVICE_MEMORY || result == VK_ERROR_OUT_OF_HOST_MEMORY) && has_count_above_one)
        {
            allocation_scale *= 0.5;
            continue;
        }

        LOG_ERROR("Failed to create bindless descriptor pool (maxSets={}, scale={}, VkResult={})", pool_create_info.maxSets,
                  global_scale * allocation_scale, static_cast<int>(result));
        return false;
    }
}

bool VulkanDescriptorSetAllocator::RebindSharedBindlessSets()
{
    std::vector<std::pair<u64, VkDescriptorSet>> new_sets;
    new_sets.reserve(allocated_bindless_descriptorsets.size());

    for (const auto &[layout_hash, state] : allocated_bindless_descriptorsets)
    {
        auto layout_it = m_descriptor_set_layout_map.find(layout_hash);
        if (layout_it == m_descriptor_set_layout_map.end())
        {
            LOG_ERROR("Missing bindless layout while rebinding shared sets (hash={})", layout_hash);
            return false;
        }

        VkDescriptorSet new_set = VK_NULL_HANDLE;
        const VkResult allocate_result = AllocateDescriptorSetWithOptionalVariableCount(
            m_context.device, m_bindless_descriptor_pool, layout_it->second, state.variable_descriptor_count, new_set);
        if (allocate_result != VK_SUCCESS)
        {
            LOG_ERROR("Failed to re-allocate shared bindless set (layout_hash={}, VkResult={})", layout_hash,
                      static_cast<int>(allocate_result));
            return false;
        }
        new_sets.emplace_back(layout_hash, new_set);
    }

    for (const auto &[layout_hash, new_set] : new_sets)
    {
        auto state_it = allocated_bindless_descriptorsets.find(layout_hash);
        if (state_it == allocated_bindless_descriptorsets.end() || state_it->second.set == nullptr)
        {
            continue;
        }

        state_it->second.set->Rebind(new_set);
        state_it->second.set->Update();
    }

    return true;
}

bool VulkanDescriptorSetAllocator::RecreateBindlessPool(bool grow_pool)
{
    const VkDescriptorPool old_pool = m_bindless_descriptor_pool;
    if (grow_pool)
    {
        m_bindless_pool_max_sets = std::max(1u, m_bindless_pool_max_sets * 2u);
    }

    m_bindless_descriptor_pool = VK_NULL_HANDLE;
    if (!EnsureBindlessPool())
    {
        m_bindless_descriptor_pool = old_pool;
        return false;
    }

    if (!RebindSharedBindlessSets())
    {
        vkDestroyDescriptorPool(m_context.device, m_bindless_descriptor_pool, nullptr);
        m_bindless_descriptor_pool = old_pool;
        return false;
    }

    if (old_pool != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(m_context.device, old_pool, nullptr);
    }
    return true;
}

void VulkanDescriptorSetAllocator::CreateDescriptorSetLayout(VulkanPipeline *pipeline)
{
    pipeline->m_pipeline_layout_desc.descriptor_set_hash_key = m_empty_descriptor_set_layout_hash_key;
    pipeline->m_pipeline_layout_desc.bindless_descriptor_set_hash_key = 0;

    const auto &rsd = pipeline->GetRootSignatureDesc();

    for (const auto &[set_number, descriptors] : rsd.descriptors)
    {
        if (set_number != DEFAULT_DESCRIPTOR_SET_NUMBER && set_number != BINDLESS_DESCRIPTOR_SET_NUMBER)
        {
            LOG_WARN("Ignoring descriptor set {} in Vulkan allocator. Current implementation only supports set0/set1.",
                     set_number);
        }
        if (descriptors.empty())
        {
            continue;
        }
    }

    if (auto default_set_it = rsd.descriptors.find(DEFAULT_DESCRIPTOR_SET_NUMBER);
        default_set_it != rsd.descriptors.end() && !default_set_it->second.empty())
    {
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        bindings.reserve(default_set_it->second.size());
        for (const auto &[name, descriptor] : default_set_it->second)
        {
            (void)name;
            VkDescriptorSetLayoutBinding binding{};
            binding.binding = descriptor.vk_binding;
            binding.descriptorCount = GetResolvedDescriptorCount(descriptor);
            binding.stageFlags = VK_SHADER_STAGE_ALL;
            binding.descriptorType = util_to_vk_descriptor_type(descriptor.type);
            bindings.push_back(binding);
        }

        std::sort(bindings.begin(), bindings.end(),
                  [](const VkDescriptorSetLayoutBinding &lhs, const VkDescriptorSetLayoutBinding &rhs) {
                      return lhs.binding < rhs.binding;
                  });

        VkDescriptorSetLayoutCreateInfo layout_create_info{};
        layout_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout_create_info.bindingCount = static_cast<u32>(bindings.size());
        layout_create_info.pBindings = bindings.data();
        layout_create_info.flags = 0;

        const u64 key = BuildDescriptorSetLayoutKey(bindings, layout_create_info.flags);
        if (m_descriptor_set_layout_map.find(key) == m_descriptor_set_layout_map.end())
        {
            VkDescriptorSetLayout layout = VK_NULL_HANDLE;
            CHECK_VK_RESULT(vkCreateDescriptorSetLayout(m_context.device, &layout_create_info, nullptr, &layout));
            m_descriptor_set_layout_map.emplace(key, layout);
        }
        pipeline->m_pipeline_layout_desc.descriptor_set_hash_key = key;
    }

    auto bindless_set_it = rsd.descriptors.find(BINDLESS_DESCRIPTOR_SET_NUMBER);
    if (bindless_set_it == rsd.descriptors.end() || bindless_set_it->second.empty())
    {
        return;
    }

    struct ResolvedBindlessDesc
    {
        std::string name;
        DescriptorDesc descriptor;
    };

    std::vector<ResolvedBindlessDesc> resolved_entries;
    resolved_entries.reserve(bindless_set_it->second.size());
    for (const auto &[name, descriptor] : bindless_set_it->second)
    {
        resolved_entries.push_back({name, descriptor});
    }

    std::sort(resolved_entries.begin(), resolved_entries.end(),
              [](const ResolvedBindlessDesc &lhs, const ResolvedBindlessDesc &rhs) {
                  return lhs.descriptor.vk_binding < rhs.descriptor.vk_binding;
              });

    std::unordered_map<VkDescriptorType, u32> fixed_usage_by_type;
    std::unordered_map<VkDescriptorType, u32> runtime_count_by_type;

    for (auto &entry : resolved_entries)
    {
        const VkDescriptorType type = util_to_vk_descriptor_type(entry.descriptor.type);
        if (entry.descriptor.is_runtime_array)
        {
            runtime_count_by_type[type] += 1;
        }
        else
        {
            entry.descriptor.descriptor_count = GetResolvedDescriptorCount(entry.descriptor);
            fixed_usage_by_type[type] += entry.descriptor.descriptor_count;
        }
    }

    for (auto &entry : resolved_entries)
    {
        if (!entry.descriptor.is_runtime_array)
        {
            continue;
        }

        const VkDescriptorType type = util_to_vk_descriptor_type(entry.descriptor.type);
        const u32 type_limit = GetUpdateAfterBindTypeLimit(type);
        const u32 fixed_usage = fixed_usage_by_type[type];
        const u32 runtime_bindings = std::max(1u, runtime_count_by_type[type]);
        const u32 available = (type_limit > fixed_usage) ? (type_limit - fixed_usage) : 1u;
        entry.descriptor.descriptor_count = std::max(1u, available / runtime_bindings);
    }

    std::vector<VkDescriptorSetLayoutBinding> bindings;
    bindings.reserve(resolved_entries.size());
    for (const auto &entry : resolved_entries)
    {
        VkDescriptorSetLayoutBinding binding{};
        binding.binding = entry.descriptor.vk_binding;
        binding.stageFlags = VK_SHADER_STAGE_ALL;
        binding.descriptorType = util_to_vk_descriptor_type(entry.descriptor.type);
        binding.descriptorCount = GetResolvedDescriptorCount(entry.descriptor);
        bindings.push_back(binding);
    }

    bool enable_variable_descriptor_count = false;
    u32 variable_descriptor_count = 1;
    if (!resolved_entries.empty() && resolved_entries.back().descriptor.is_runtime_array)
    {
        enable_variable_descriptor_count = true;
        variable_descriptor_count = std::max(1u, resolved_entries.back().descriptor.descriptor_count);
    }
    else
    {
        for (const auto &entry : resolved_entries)
        {
            if (entry.descriptor.is_runtime_array)
            {
                LOG_WARN("Bindless set {} contains runtime arrays, but highest binding is not runtime; "
                         "VARIABLE_DESCRIPTOR_COUNT disabled.",
                         BINDLESS_DESCRIPTOR_SET_NUMBER);
                break;
            }
        }
    }

    if (!CheckBindlessFeatureSupport(bindings, enable_variable_descriptor_count))
    {
        LOG_ERROR("Disable bindless descriptor set layout for pipeline {} due to unsupported descriptor indexing features",
                  static_cast<const void *>(pipeline));
        pipeline->m_pipeline_layout_desc.bindless_descriptor_set_hash_key = 0;
        return;
    }

    std::vector<VkDescriptorBindingFlags> binding_flags(
        bindings.size(), VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT);
    if (enable_variable_descriptor_count && !binding_flags.empty())
    {
        binding_flags.back() |= VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT;
    }

    VkDescriptorSetLayoutBindingFlagsCreateInfo binding_flags_info{};
    binding_flags_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT;
    binding_flags_info.bindingCount = static_cast<u32>(binding_flags.size());
    binding_flags_info.pBindingFlags = binding_flags.data();

    VkDescriptorSetLayoutCreateInfo layout_create_info{};
    layout_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_create_info.bindingCount = static_cast<u32>(bindings.size());
    layout_create_info.pBindings = bindings.data();
    layout_create_info.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT;
    layout_create_info.pNext = &binding_flags_info;

    const u64 key = BuildDescriptorSetLayoutKey(bindings, layout_create_info.flags, &binding_flags);
    if (m_descriptor_set_layout_map.find(key) == m_descriptor_set_layout_map.end())
    {
        VkDescriptorSetLayout layout = VK_NULL_HANDLE;
        CHECK_VK_RESULT(vkCreateDescriptorSetLayout(m_context.device, &layout_create_info, nullptr, &layout));
        m_descriptor_set_layout_map.emplace(key, layout);
    }

    BindlessLayoutMeta meta{};
    meta.variable_descriptor_count = variable_descriptor_count;
    for (const auto &entry : resolved_entries)
    {
        meta.write_descs[entry.name] = entry.descriptor;
    }
    m_bindless_layout_meta[key] = meta;
    pipeline->m_pipeline_layout_desc.bindless_descriptor_set_hash_key = key;
}
VulkanDescriptorSet *VulkanDescriptorSetAllocator::GetDescriptorSet(VulkanPipeline *pipeline)
{
    if (pipeline->m_pipeline_layout_desc.descriptor_set_hash_key == 0)
    {
        return nullptr;
    }

    auto cached_it = allocated_descriptorsets.find(pipeline);
    if (cached_it != allocated_descriptorsets.end() && cached_it->second.set)
    {
        return cached_it->second.set;
    }

    auto write_descs_it = pipeline->GetRootSignatureDesc().descriptors.find(DEFAULT_DESCRIPTOR_SET_NUMBER);
    if (write_descs_it == pipeline->GetRootSignatureDesc().descriptors.end() || write_descs_it->second.empty())
    {
        return nullptr;
    }

    auto write_descs = write_descs_it->second;
    for (auto &[name, descriptor] : write_descs)
    {
        (void)name;
        descriptor.descriptor_count = GetResolvedDescriptorCount(descriptor);
        descriptor.is_runtime_array = false;
    }

    const auto type_counts = BuildPerSetTypeCounts(write_descs);
    if (type_counts.empty())
    {
        return nullptr;
    }

    const VkDescriptorSetLayout layout = GetVkDescriptorSetLayout(pipeline->m_pipeline_layout_desc.descriptor_set_hash_key);
    if (layout == VK_NULL_HANDLE)
    {
        LOG_ERROR("Missing descriptor set layout for set0 (hash={})",
                  pipeline->m_pipeline_layout_desc.descriptor_set_hash_key);
        return nullptr;
    }

    VkDescriptorSet vk_ds = VK_NULL_HANDLE;
    size_t pool_index = std::numeric_limits<size_t>::max();
    for (size_t i = 0; i < m_default_pools.size(); ++i)
    {
        VkDescriptorSetAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool = m_default_pools[i].pool;
        alloc_info.descriptorSetCount = 1;
        alloc_info.pSetLayouts = &layout;

        const VkResult result = vkAllocateDescriptorSets(m_context.device, &alloc_info, &vk_ds);
        if (result == VK_SUCCESS)
        {
            pool_index = i;
            break;
        }
        if (!IsRecoverableAllocateError(result))
        {
            LOG_ERROR("Failed to allocate set0 descriptor set (pool={}, VkResult={})", i, static_cast<int>(result));
            return nullptr;
        }
    }

    if (pool_index == std::numeric_limits<size_t>::max())
    {
        const u32 next_pool_sets =
            m_default_pools.empty() ? std::max(1u, static_cast<u32>(allocated_descriptorsets.size() + 1))
                                    : std::max(1u, m_default_pools.back().max_sets * 2u);

        VkDescriptorPool new_pool = VK_NULL_HANDLE;
        if (!CreateDefaultPool(type_counts, next_pool_sets, new_pool))
        {
            return nullptr;
        }

        m_default_pools.push_back(DefaultPoolState{new_pool, next_pool_sets, type_counts});
        pool_index = m_default_pools.size() - 1;

        VkDescriptorSetAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool = new_pool;
        alloc_info.descriptorSetCount = 1;
        alloc_info.pSetLayouts = &layout;
        const VkResult result = vkAllocateDescriptorSets(m_context.device, &alloc_info, &vk_ds);
        if (result != VK_SUCCESS)
        {
            LOG_ERROR("Failed to allocate set0 descriptor set from grown pool (VkResult={})", static_cast<int>(result));
            return nullptr;
        }
    }

    auto *set = new VulkanDescriptorSet(m_context, DEFAULT_DESCRIPTOR_SET_NUMBER, std::move(write_descs), vk_ds);
    allocated_descriptorsets[pipeline] = DefaultSetAllocation{set, pool_index};
    return set;
}

VulkanDescriptorSet *VulkanDescriptorSetAllocator::GetBindlessDescriptorSet(VulkanPipeline *pipeline)
{
    const u64 layout_hash = pipeline->m_pipeline_layout_desc.bindless_descriptor_set_hash_key;
    if (layout_hash == 0)
    {
        return nullptr;
    }

    auto pipeline_layout_it = m_pipeline_bindless_layout_map.find(pipeline);
    if (pipeline_layout_it == m_pipeline_bindless_layout_map.end())
    {
        m_pipeline_bindless_layout_map[pipeline] = layout_hash;
        m_bindless_layout_ref_count[layout_hash] += 1;
    }
    else if (pipeline_layout_it->second != layout_hash)
    {
        const u64 previous_layout_hash = pipeline_layout_it->second;
        auto previous_ref_it = m_bindless_layout_ref_count.find(previous_layout_hash);
        if (previous_ref_it != m_bindless_layout_ref_count.end())
        {
            if (previous_ref_it->second > 0)
            {
                previous_ref_it->second -= 1;
            }
            if (previous_ref_it->second == 0)
            {
                m_bindless_layout_ref_count.erase(previous_ref_it);
            }
        }

        pipeline_layout_it->second = layout_hash;
        m_bindless_layout_ref_count[layout_hash] += 1;
    }

    auto cached_it = allocated_bindless_descriptorsets.find(layout_hash);
    if (cached_it != allocated_bindless_descriptorsets.end() && cached_it->second.set)
    {
        return cached_it->second.set;
    }

    auto layout_it = m_descriptor_set_layout_map.find(layout_hash);
    auto meta_it = m_bindless_layout_meta.find(layout_hash);
    if (layout_it == m_descriptor_set_layout_map.end() || meta_it == m_bindless_layout_meta.end())
    {
        LOG_ERROR("Missing bindless layout/meta for hash={}", layout_hash);
        return nullptr;
    }

    m_bindless_pool_max_sets =
        std::max(m_bindless_pool_max_sets, static_cast<u32>(allocated_bindless_descriptorsets.size() + 1));

    if (!EnsureBindlessPool())
    {
        return nullptr;
    }

    VkDescriptorSet vk_ds = VK_NULL_HANDLE;
    VkResult allocate_result = AllocateDescriptorSetWithOptionalVariableCount(
        m_context.device, m_bindless_descriptor_pool, layout_it->second, meta_it->second.variable_descriptor_count, vk_ds);

    if (IsRecoverableAllocateError(allocate_result))
    {
        if (!RecreateBindlessPool(true))
        {
            LOG_ERROR("Failed to grow/recreate bindless descriptor pool");
            return nullptr;
        }

        allocate_result = AllocateDescriptorSetWithOptionalVariableCount(
            m_context.device, m_bindless_descriptor_pool, layout_it->second, meta_it->second.variable_descriptor_count,
            vk_ds);
    }

    if (allocate_result != VK_SUCCESS)
    {
        LOG_ERROR("Failed to allocate bindless descriptor set (layout_hash={}, VkResult={})", layout_hash,
                  static_cast<int>(allocate_result));
        return nullptr;
    }

    auto *set = new VulkanDescriptorSet(m_context, BINDLESS_DESCRIPTOR_SET_NUMBER, meta_it->second.write_descs, vk_ds);
    allocated_bindless_descriptorsets[layout_hash] = BindlessSetState{set, meta_it->second.variable_descriptor_count};
    return set;
}

VulkanDescriptorSetAllocator::~VulkanDescriptorSetAllocator() noexcept
{
    for (auto &[pipeline, allocation] : allocated_descriptorsets)
    {
        (void)pipeline;
        delete allocation.set;
    }
    allocated_descriptorsets.clear();

    for (auto &[layout_hash, state] : allocated_bindless_descriptorsets)
    {
        (void)layout_hash;
        delete state.set;
    }
    allocated_bindless_descriptorsets.clear();
    m_bindless_layout_ref_count.clear();
    m_pipeline_bindless_layout_map.clear();

    for (auto &[key, layout] : m_descriptor_set_layout_map)
    {
        (void)key;
        vkDestroyDescriptorSetLayout(m_context.device, layout, nullptr);
    }
    m_descriptor_set_layout_map.clear();

    for (const auto &pool_state : m_default_pools)
    {
        if (pool_state.pool != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorPool(m_context.device, pool_state.pool, nullptr);
        }
    }
    m_default_pools.clear();

    if (m_bindless_descriptor_pool != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(m_context.device, m_bindless_descriptor_pool, nullptr);
        m_bindless_descriptor_pool = VK_NULL_HANDLE;
    }
}

void VulkanDescriptorSetAllocator::CreateDescriptorPool()
{
    if (!m_default_pools.empty())
    {
        return;
    }

    std::unordered_map<VkDescriptorType, u32> default_counts{
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},
        {VK_DESCRIPTOR_TYPE_SAMPLER, 1},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1},
    };

    VkDescriptorPool pool = VK_NULL_HANDLE;
    if (!CreateDefaultPool(default_counts, 1, pool))
    {
        return;
    }
    m_default_pools.push_back(DefaultPoolState{pool, 1, default_counts});
}

void VulkanDescriptorSetAllocator::CreateBindlessDescriptorPool()
{
    (void)EnsureBindlessPool();
}

void VulkanDescriptorSetAllocator::ResetDescriptorPool()
{
    for (auto &[pipeline, allocation] : allocated_descriptorsets)
    {
        (void)pipeline;
        delete allocation.set;
    }
    allocated_descriptorsets.clear();

    for (const auto &pool_state : m_default_pools)
    {
        if (pool_state.pool != VK_NULL_HANDLE)
        {
            const VkResult reset_result = vkResetDescriptorPool(m_context.device, pool_state.pool, 0);
            if (reset_result != VK_SUCCESS)
            {
                LOG_ERROR("Failed to reset default descriptor pool (VkResult={})", static_cast<int>(reset_result));
            }
        }
    }
}

void VulkanDescriptorSetAllocator::ReleaseDescriptorSets(VulkanPipeline *pipeline)
{
    auto default_it = allocated_descriptorsets.find(pipeline);
    if (default_it != allocated_descriptorsets.end())
    {
        delete default_it->second.set;
        allocated_descriptorsets.erase(default_it);
    }

    auto pipeline_layout_it = m_pipeline_bindless_layout_map.find(pipeline);
    if (pipeline_layout_it == m_pipeline_bindless_layout_map.end())
    {
        return;
    }

    const u64 layout_hash = pipeline_layout_it->second;
    m_pipeline_bindless_layout_map.erase(pipeline_layout_it);

    auto ref_it = m_bindless_layout_ref_count.find(layout_hash);
    if (ref_it == m_bindless_layout_ref_count.end())
    {
        return;
    }
    if (ref_it->second > 0)
    {
        ref_it->second -= 1;
    }
    if (ref_it->second != 0)
    {
        return;
    }
    m_bindless_layout_ref_count.erase(ref_it);

    auto bindless_set_it = allocated_bindless_descriptorsets.find(layout_hash);
    if (bindless_set_it != allocated_bindless_descriptorsets.end())
    {
        delete bindless_set_it->second.set;
        allocated_bindless_descriptorsets.erase(bindless_set_it);
    }
}

VkDescriptorSetLayout VulkanDescriptorSetAllocator::GetVkDescriptorSetLayout(u64 key) const
{
    auto layout_it = m_descriptor_set_layout_map.find(key);
    if (layout_it == m_descriptor_set_layout_map.end())
    {
        LOG_ERROR("Descriptor set layout key {} not found", key);
        return VK_NULL_HANDLE;
    }
    return layout_it->second;
}

} // namespace Horizon::Backend
