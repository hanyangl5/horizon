#include "pipeline.h"

#include <algorithm>
#include <core/log.h>

namespace Horizon::Backend
{

Pipeline::Pipeline() noexcept
{
}

Pipeline::~Pipeline() noexcept
{
}

PipelineType Pipeline::GetType() const noexcept
{
    return m_type;
}

PrimitiveTopology Pipeline::GetTopology() const noexcept
{
    return m_topology;
}

void Pipeline::ParseRootSignature(const ShaderPrograms &shaders)
{
    if (m_type == PipelineType::GRAPHICS)
    {
        if (shaders.VertexShader() != nullptr)
        {
            ParseRootSignatureFromShader((Shader *)shaders.VertexShader());
        }
        if (shaders.TaskShader() != nullptr)
        {
            ParseRootSignatureFromShader((Shader *)shaders.TaskShader());
        }
        if (shaders.MeshShader() != nullptr)
        {
            ParseRootSignatureFromShader((Shader *)shaders.MeshShader());
        }
        if (shaders.PixelShader() != nullptr)
        {
            ParseRootSignatureFromShader((Shader *)shaders.PixelShader());
        }
    }
    else if (m_type == PipelineType::COMPUTE)
    {
        ParseRootSignatureFromShader((Shader *)shaders.ComputeShader());
    }
}

void Pipeline::ParseRootSignatureFromShader(Shader *shader)
{
    if (shader == nullptr)
    {
        LOG_ERROR("ParseRootSignatureFromShader received null shader");
        return;
    }

    const RootSignatureDesc *refl = shader->GetReflectionData();
    if (!refl)
        return;

    // Merge descriptors from shader reflection, using set number directly.
    // Resource names are used by the runtime as lookup keys, so enforce
    // consistent set/binding/type for the same name across stages.
    for (const auto &[set_number, descriptors] : refl->descriptors)
    {
        for (const auto &[name, desc] : descriptors)
        {
            bool conflict = false;
            for (const auto &[existing_set, existing_descriptors] : rsd.descriptors)
            {
                auto existing_name_it = existing_descriptors.find(name);
                if (existing_name_it == existing_descriptors.end())
                {
                    continue;
                }

                const auto &existing = existing_name_it->second;
                if (existing_set != set_number || existing.vk_binding != desc.vk_binding || existing.type != desc.type)
                {
                    LOG_ERROR("Descriptor '{}' reflection mismatch across shader stages: existing(set={}, binding={}, "
                              "type={}), incoming(set={}, binding={}, type={})",
                              name, existing_set, existing.vk_binding, static_cast<u32>(existing.type), set_number,
                              desc.vk_binding, static_cast<u32>(desc.type));
                    conflict = true;
                }
                break;
            }
            if (conflict)
            {
                continue;
            }

            auto &set_descriptors = rsd.descriptors[set_number];
            auto existing_it = set_descriptors.find(name);
            if (existing_it == set_descriptors.end())
            {
                DescriptorDesc merged = desc;
                if (!merged.is_runtime_array && merged.descriptor_count == 0)
                {
                    merged.descriptor_count = 1;
                }
                set_descriptors.emplace(name, merged);
                continue;
            }

            auto &existing = existing_it->second;
            if (existing.vk_binding != desc.vk_binding || existing.type != desc.type)
            {
                LOG_ERROR("Descriptor '{}' reflection mismatch in set {}: existing(binding={}, type={}), "
                          "incoming(binding={}, type={})",
                          name, set_number, existing.vk_binding, static_cast<u32>(existing.type), desc.vk_binding,
                          static_cast<u32>(desc.type));
                continue;
            }

            existing.is_runtime_array = existing.is_runtime_array || desc.is_runtime_array;
            if (existing.is_runtime_array)
            {
                existing.descriptor_count = 0;
            }
            else
            {
                const u32 incoming_count = std::max(1u, desc.descriptor_count);
                existing.descriptor_count = std::max(existing.descriptor_count, incoming_count);
            }
        }
    }

    // Merge push constants
    for (const auto &[pc_name, pc] : refl->push_constants)
    {
        auto it = rsd.push_constants.find(pc_name);
        if (it != rsd.push_constants.end())
        {
            it->second.shader_stages |= pc.shader_stages;
            if (it->second.binding == 0xFFFFFFFFu && pc.binding != 0xFFFFFFFFu)
            {
                it->second.binding = pc.binding;
            }
            if (it->second.set == 0xFFFFFFFFu && pc.set != 0xFFFFFFFFu)
            {
                it->second.set = pc.set;
            }
        }
        else
        {
            rsd.push_constants.emplace(pc_name, pc);
        }
    }
}

} // namespace Horizon::Backend
