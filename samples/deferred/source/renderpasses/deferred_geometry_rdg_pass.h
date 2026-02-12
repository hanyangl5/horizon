#pragma once
#include "../header.h"
#include "../scene.h"
#include <render_graph/frame_graph.h>

class DeferredShadingGeometryPass : public Horizon::Backend::RDGPass
{
  public:
    DeferredShadingGeometryPass(RHI *rhi, Horizon::SceneManager *scene_manager, Sampler *sampler, u32 width, u32 height);
    ~DeferredShadingGeometryPass();

    void ImportResources(Horizon::Backend::FrameGraph *frame_graph) override;
    void Setup(Horizon::Backend::FrameGraphBuilder &builder) override;
    void Execute(CommandList *command_list, Horizon::Backend::FrameGraphBuilder &builder) override;

    // Accessors for resource handles
    Horizon::Backend::TextureHandle GetGBuffer0Handle() const
    {
        return m_gbuffer0_handle;
    }
    Horizon::Backend::TextureHandle GetGBuffer1Handle() const
    {
        return m_gbuffer1_handle;
    }
    Horizon::Backend::TextureHandle GetGBuffer2Handle() const
    {
        return m_gbuffer2_handle;
    }
    Horizon::Backend::TextureHandle GetGBuffer3Handle() const
    {
        return m_gbuffer3_handle;
    }
    Horizon::Backend::TextureHandle GetGBuffer4Handle() const
    {
        return m_gbuffer4_handle;
    }
    Horizon::Backend::TextureHandle GetDepthHandle() const
    {
        return m_depth_handle;
    }

  private:
    RHI *m_rhi;
    Horizon::SceneManager *m_scene_manager;
    Sampler *m_sampler;
    u32 m_width;
    u32 m_height;
    Buffer *m_taa_prev_curr_offset_buffer;

    // Pipeline resources (owned by pass, not FrameGraph)
    Shader *m_geometry_vs;
    Shader *m_geometry_ps;
    Pipeline *m_geometry_pipeline;

    // Render targets (owned by pass)
    RenderTarget *m_gbuffer0_rt;
    RenderTarget *m_gbuffer1_rt;
    RenderTarget *m_gbuffer2_rt;
    RenderTarget *m_gbuffer3_rt;
    RenderTarget *m_gbuffer4_rt;
    RenderTarget *m_depth_rt;

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
