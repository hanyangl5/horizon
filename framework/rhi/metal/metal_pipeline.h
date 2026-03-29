#pragma once

#include <rhi/metal/metal_utils.h>

namespace Horizon::Backend
{

class MetalPipeline final : public Pipeline
{
  public:
    MetalPipeline() noexcept;
    ~MetalPipeline() noexcept override;
    MetalPipeline(const MetalPipeline &rhs) noexcept = delete;
    MetalPipeline &operator=(const MetalPipeline &rhs) noexcept = delete;
    MetalPipeline(MetalPipeline &&rhs) noexcept = delete;
    MetalPipeline &operator=(MetalPipeline &&rhs) noexcept = delete;

    void SetResource(Buffer *resource, const std::string &resource_name) override;
    void SetResource(Texture *resource, const std::string &resource_name) override;
    void SetResource(Sampler *resource, const std::string &resource_name) override;
    void SetBindlessResource(std::vector<Buffer *> &resource, const std::string &resource_name) override;
    void SetBindlessResource(std::vector<Texture *> &resource, const std::string &resource_name) override;

    void SetTopology(PrimitiveTopology topology) noexcept;

    MTL::RenderPipelineState *m_render_pipeline_state{};
    MTL::DepthStencilState *m_depth_stencil_state{};
    MTL::PrimitiveType m_primitive_type{MTL::PrimitiveTypeTriangle};
    MTL::Viewport m_viewport{};
    MTL::Winding m_front_winding{MTL::WindingCounterClockwise};
    MTL::CullMode m_cull_mode{MTL::CullModeNone};
    MTL::TriangleFillMode m_fill_mode{MTL::TriangleFillModeFill};
};

} // namespace Horizon::Backend
