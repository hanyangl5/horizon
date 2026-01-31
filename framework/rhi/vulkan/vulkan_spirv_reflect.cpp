#include "vulkan_spirv_reflect.h"
#include "vulkan_utils.h"

#include <core/log.h>
#include <rhi/enums.h>

#define SPIRV_REFLECT_USE_SYSTEM_SPIRV_H
#include <spirv_reflect.h>

#include <cstring>
#include <vector>

namespace Horizon::Backend {

namespace {

u32 spv_stage_to_internal(SpvReflectShaderStageFlagBits stage) noexcept {
    u32 s = 0;
    if (stage & SPV_REFLECT_SHADER_STAGE_VERTEX_BIT)
        s |= static_cast<u32>(ShaderStageFlags::SHADER_STAGE_VERTEX_SHADER);
    if (stage & SPV_REFLECT_SHADER_STAGE_FRAGMENT_BIT)
        s |= static_cast<u32>(ShaderStageFlags::SHADER_STAGE_PIXEL_SHADER);
    if (stage & SPV_REFLECT_SHADER_STAGE_COMPUTE_BIT)
        s |= static_cast<u32>(ShaderStageFlags::SHADER_STAGE_COMPUTE_SHADER);
    return s;
}

} // namespace

void ReflectSpirvToRootSignature(const void *spirv, size_t size, ShaderType stage,
                                 RootSignatureDesc &out) noexcept {
    SpvReflectShaderModule module = {};
    SpvReflectResult res = spvReflectCreateShaderModule(size, spirv, &module);
    if (res != SPV_REFLECT_RESULT_SUCCESS) {
        LOG_ERROR("spvReflectCreateShaderModule failed");
        return;
    }

    u32 stage_bits = spv_stage_to_internal(static_cast<SpvReflectShaderStageFlagBits>(module.shader_stage));
    if (stage_bits == 0)
        stage_bits = spv_stage_to_internal(SPV_REFLECT_SHADER_STAGE_VERTEX_BIT);
    if (stage == ShaderType::PIXEL_SHADER)
        stage_bits = static_cast<u32>(ShaderStageFlags::SHADER_STAGE_PIXEL_SHADER);
    else if (stage == ShaderType::COMPUTE_SHADER)
        stage_bits = static_cast<u32>(ShaderStageFlags::SHADER_STAGE_COMPUTE_SHADER);
    else if (stage == ShaderType::VERTEX_SHADER)
        stage_bits = static_cast<u32>(ShaderStageFlags::SHADER_STAGE_VERTEX_SHADER);

    uint32_t binding_count = 0;
    spvReflectEnumerateDescriptorBindings(&module, &binding_count, nullptr);
    if (binding_count > 0) {
        std::vector<SpvReflectDescriptorBinding *> bindings(binding_count);
        spvReflectEnumerateDescriptorBindings(&module, &binding_count, bindings.data());
        for (uint32_t i = 0; i < binding_count; ++i) {
            const SpvReflectDescriptorBinding *b = bindings[i];
            u32 set = b->set; // Use shader's set number directly
            DescriptorDesc desc{};
            desc.type = vk_to_descriptor_type(static_cast<VkDescriptorType>(b->descriptor_type));
            desc.vk_binding = b->binding;
            const char *name = b->name && b->name[0] ? b->name : "?";
            out.descriptors[set].try_emplace(std::string(name), desc);
        }
    }

    uint32_t block_count = 0;
    spvReflectEnumeratePushConstantBlocks(&module, &block_count, nullptr);
    if (block_count > 0) {
        std::vector<SpvReflectBlockVariable *> blocks(block_count);
        spvReflectEnumeratePushConstantBlocks(&module, &block_count, blocks.data());
        for (uint32_t i = 0; i < block_count; ++i) {
            const SpvReflectBlockVariable *pc = blocks[i];
            PushConstantDesc pcd{};
            pcd.size = pc->size;
            pcd.offset = pc->offset;
            pcd.shader_stages = stage_bits;
            const char *name = pc->name && pc->name[0] ? pc->name : "push";
            out.push_constants.emplace(std::string(name), pcd);
        }
    }

    spvReflectDestroyShaderModule(&module);
}

} // namespace Horizon::Backend
