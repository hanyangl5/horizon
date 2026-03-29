#include "metal_pipeline.h"

namespace Horizon::Backend
{

MetalPipeline::MetalPipeline() noexcept
{
    m_type = PipelineType::GRAPHICS;
}

MetalPipeline::~MetalPipeline() noexcept
{
    if (m_render_pipeline_state != nil)
    {
        m_render_pipeline_state->release();
        m_render_pipeline_state = nil;
    }
    if (m_depth_stencil_state != nil)
    {
        m_depth_stencil_state->release();
        m_depth_stencil_state = nil;
    }
}

void MetalPipeline::SetResource(Buffer *resource, const std::string &resource_name)
{
    (void)resource;
    (void)resource_name;
}

void MetalPipeline::SetResource(Texture *resource, const std::string &resource_name)
{
    (void)resource;
    (void)resource_name;
}

void MetalPipeline::SetResource(Sampler *resource, const std::string &resource_name)
{
    (void)resource;
    (void)resource_name;
}

void MetalPipeline::SetBindlessResource(std::vector<Buffer *> &resource, const std::string &resource_name)
{
    (void)resource;
    (void)resource_name;
}

void MetalPipeline::SetBindlessResource(std::vector<Texture *> &resource, const std::string &resource_name)
{
    (void)resource;
    (void)resource_name;
}

void MetalPipeline::SetTopology(PrimitiveTopology topology) noexcept
{
    m_topology = topology;
}

} // namespace Horizon::Backend
