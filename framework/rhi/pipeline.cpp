#include "pipeline.h"

#include <algorithm>

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
    return m_create_info.type;
}

void Pipeline::ParseRootSignature()
{
    if (m_create_info.type == PipelineType::GRAPHICS)
    {
        ParseRootSignatureFromShader(m_vs);
        ParseRootSignatureFromShader(m_ps);
    }
    else if (m_create_info.type == PipelineType::COMPUTE)
    {
        ParseRootSignatureFromShader(m_cs);
    }
}

void Pipeline::ParseRootSignatureFromShader(Shader *shader)
{
    const RootSignatureDesc *refl = shader->GetReflectionData();
    if (!refl)
        return;

    // Merge descriptors from shader reflection, using set number directly
    for (const auto &[set_number, descriptors] : refl->descriptors)
    {
        for (const auto &[name, desc] : descriptors)
        {
            rsd.descriptors[set_number].try_emplace(name, desc);
        }
    }

    // Merge push constants
    for (const auto &[pc_name, pc] : refl->push_constants)
    {
        auto it = rsd.push_constants.find(pc_name);
        if (it != rsd.push_constants.end())
        {
            it->second.shader_stages |= pc.shader_stages;
        }
        else
        {
            rsd.push_constants.emplace(pc_name, pc);
        }
    }
}

} // namespace Horizon::Backend
