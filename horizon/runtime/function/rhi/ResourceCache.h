#pragma once
#include <runtime/core/utils/definations.h>
#include <runtime/function/rhi/RHIUtils.h>
#include <vulkan/vulkan.h>

namespace std {

template <typename T> inline void hash_combine(std::size_t &seed, const T &val) {
    seed ^= std::hash<T>()(val) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

template <> struct hash<VkDescriptorSetLayoutCreateInfo> {
    inline Horizon::u64 operator()(VkDescriptorSetLayoutCreateInfo const &layout) const {
        std::size_t seed = 0;
        hash_combine(seed, layout.bindingCount);
        for (Horizon::u32 i = 0; i < layout.bindingCount; i++) {
            hash_combine(seed, layout.pBindings[i].binding);
            hash_combine(seed, layout.pBindings[i].descriptorCount);
            hash_combine(seed, layout.pBindings[i].stageFlags);
            hash_combine(seed, layout.pBindings[i].descriptorType);
        }
        return seed;
    }
};
template <> struct hash<Horizon::GraphicsPipelineCreateInfo> {
    inline Horizon::u64 operator()(const Horizon::GraphicsPipelineCreateInfo &create_info) const {
        std::size_t seed = 0;
        hash_combine(seed, create_info.depth_stencil_state.depthRange[0]);
        hash_combine(seed, create_info.depth_stencil_state.depthRange[1]);
        hash_combine(seed, create_info.depth_stencil_state.depth_func);
        hash_combine(seed, create_info.depth_stencil_state.depth_stencil_format);
        hash_combine(seed, create_info.depth_stencil_state.depth_test);
        hash_combine(seed, create_info.depth_stencil_state.depth_write);
        //
        return seed;
    }
};
template <> struct hash<Horizon::ComputePipelineCreateInfo> {
    inline Horizon::u64 operator()(const Horizon::ComputePipelineCreateInfo &create_info) const {
        std::size_t seed = 0;
        //
        return seed;
    }
};

template <> struct hash<Horizon::PipelineCreateInfo> {
    inline Horizon::u64 operator()(const Horizon::PipelineCreateInfo &create_info) const {
        std::size_t seed = 0;
        switch (create_info.type) {
        case Horizon::PipelineType::GRAPHICS:
            hash_combine(seed, *create_info.gpci);
            break;
        case Horizon::PipelineType::COMPUTE:
            hash_combine(seed, *create_info.cpci);
        default:
            break;
        };
        hash_combine(seed, create_info.type);
        return seed;
    }
};

} // namespace std