#pragma once
#include "../header.h"
#include <render_graph/frame_graph.h>

class GTAORDGPass : public Horizon::Backend::RDGPass
{
  public:
    GTAORDGPass(RHI *rhi, Sampler *sampler, u32 width, u32 height);
    ~GTAORDGPass();

    void ImportResources(Horizon::Backend::FrameGraph *frame_graph) override;
    void Setup(Horizon::Backend::FrameGraphBuilder &builder) override;
    void Execute(CommandList *command_list, Horizon::Backend::FrameGraphBuilder &builder) override;

    void SetInputHandles(Horizon::Backend::TextureHandle depth, Horizon::Backend::TextureHandle gbuffer0);
    Buffer *GetConstantsBuffer() const
    {
        return m_gtao_constants_buffer;
    }
    void UpdateConstants(const void *data, u32 size);
    Horizon::Backend::TextureHandle GetGTAOFactorHandle() const
    {
        return m_gtao_factor_handle;
    }

    struct GTAOConstant
    {
        Math::float4x4 proj;
        Math::float4x4 inv_proj;
        Math::float4x4 view;
        u32 width;
        u32 height;
        f32 radius;
        f32 falloff;
        f32 thickness;
        f32 bias;
        u32 direction_count;
        u32 step_count;
        f32 max_pixel_radius;
        f32 intensity;
    };
    GTAOConstant &GetGTAOConstants()
    {
        return m_gtao_constants;
    }

  private:
    RHI *m_rhi;
    Sampler *m_sampler;
    u32 m_width;
    u32 m_height;

    Shader *m_gtao_cs;
    Pipeline *m_gtao_pipeline;

    Texture *m_gtao_factor_image;
    Buffer *m_gtao_constants_buffer;
    GTAOConstant m_gtao_constants{};

    Horizon::Backend::TextureHandle m_depth_handle;
    Horizon::Backend::TextureHandle m_gbuffer0_handle;
    Horizon::Backend::TextureHandle m_gtao_factor_handle;
};
