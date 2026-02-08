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
    return m_type;
}

void Pipeline::ParseRootSignature(const ShaderPrograms& shaders)
{
    if (m_type == PipelineType::GRAPHICS)
    {
        ParseRootSignatureFromShader((Shader*)shaders.VertexShader());
        ParseRootSignatureFromShader((Shader *)shaders.PixelShader());
    }
    else if (m_type == PipelineType::COMPUTE)
    {
        ParseRootSignatureFromShader((Shader *)shaders.ComputeShader());
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
