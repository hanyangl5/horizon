#pragma once

#include "header.h"
#include "scene.h"
#include <render_graph/frame_graph.h>

class GeometryPass : public Horizon::Backend::RDGPass
{
  public:
    GeometryPass(RHI *rhi, Horizon::SceneManager *scene_manager, Sampler *sampler, Buffer *taa_prev_curr_offset_buffer);
    ~GeometryPass() override = default;

    void ImportResources(Horizon::Backend::FrameGraph *frame_graph) override;
    void Setup(Horizon::Backend::FrameGraphBuilder &builder) override;
    void Execute(CommandList *command_list, Horizon::Backend::FrameGraphBuilder &builder) override;

  private:
    RHI *m_rhi;
    Horizon::SceneManager *m_scene_manager;
    Sampler *m_sampler;
    Buffer *m_taa_prev_curr_offset_buffer;

    // Pipeline resources (owned by pass, not FrameGraph)
    Shader *m_geometry_vs;
    Shader *m_geometry_ps;
    Pipeline *m_geometry_pipeline;

    // Resource handles (managed by FrameGraph)
    Horizon::Backend::RenderTargetHandle m_gbuffer0_rt_handle;
    Horizon::Backend::RenderTargetHandle m_gbuffer1_rt_handle;
    Horizon::Backend::RenderTargetHandle m_gbuffer2_rt_handle;
    Horizon::Backend::RenderTargetHandle m_gbuffer3_rt_handle;
    Horizon::Backend::RenderTargetHandle m_gbuffer4_rt_handle;
    Horizon::Backend::RenderTargetHandle m_depth_rt_handle;
    Horizon::Backend::TextureHandle m_gbuffer0_handle;
    Horizon::Backend::TextureHandle m_gbuffer1_handle;
    Horizon::Backend::TextureHandle m_gbuffer2_handle;
    Horizon::Backend::TextureHandle m_gbuffer3_handle;
    Horizon::Backend::TextureHandle m_gbuffer4_handle;
    Horizon::Backend::TextureHandle m_depth_handle;
};
