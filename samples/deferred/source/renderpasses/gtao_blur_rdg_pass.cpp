#include "gtao_blur_rdg_pass.h"

GTAOBlurRDGPass::GTAOBlurRDGPass(RHI *rhi, u32 width, u32 height)
    : RDGPass("GTAO Blur Pass", rhi), m_rhi(rhi), m_width(width), m_height(height)
{
    m_gtao_blur_cs = CreateShader(ShaderType::COMPUTE_SHADER, shader_dir / "gtao_blur.comp.hlsl", "main");
    ComputePipelineCreateInfo create_info{};
    create_info.shader_program.SetShader(ShaderType::COMPUTE_SHADER, m_gtao_blur_cs);
    m_gtao_blur_pipeline = CreateComputePipeline(create_info);

    CreateResizableTexture(
        m_gtao_blur_image,
        TextureCreateInfo{DescriptorType::DESCRIPTOR_TYPE_RW_TEXTURE | DescriptorType::DESCRIPTOR_TYPE_TEXTURE,
                          ResourceState::RESOURCE_STATE_UNORDERED_ACCESS, TextureType::TEXTURE_TYPE_2D,
                          TextureFormat::TEXTURE_FORMAT_R8_UNORM, m_width, m_height, 1, false, 1, "gtao_blur_image"});
    SetResizeCallback([this](u32 width, u32 height) {
        m_width = width;
        m_height = height;
    });
}

GTAOBlurRDGPass::~GTAOBlurRDGPass()
{
    DestroyShader(m_gtao_blur_cs);
    DestroyPipeline(m_gtao_blur_pipeline);
    m_rhi->DestroyTexture(m_gtao_blur_image);
}

void GTAOBlurRDGPass::ImportResources(Horizon::Backend::FrameGraph *frame_graph)
{
    m_gtao_blur_handle = frame_graph->ImportTexture("gtao_blur", m_gtao_blur_image);
}

void GTAOBlurRDGPass::SetInputHandles(Horizon::Backend::TextureHandle gtao_factor, Horizon::Backend::TextureHandle depth,
                                      Horizon::Backend::TextureHandle gbuffer0)
{
    m_gtao_factor_handle = gtao_factor;
    m_depth_handle = depth;
    m_gbuffer0_handle = gbuffer0;
}

void GTAOBlurRDGPass::Setup(Horizon::Backend::FrameGraphBuilder &builder)
{
    builder.ReadTexture(m_gtao_factor_handle, ResourceState::RESOURCE_STATE_SHADER_RESOURCE);
    builder.ReadTexture(m_depth_handle, ResourceState::RESOURCE_STATE_SHADER_RESOURCE);
    builder.ReadTexture(m_gbuffer0_handle, ResourceState::RESOURCE_STATE_SHADER_RESOURCE);
    builder.WriteTexture(m_gtao_blur_handle, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);
}

void GTAOBlurRDGPass::Execute(CommandList *cl, Horizon::Backend::FrameGraphBuilder &builder)
{
    cl->BeginComputePass("GTAO Blur Pass");
    m_gtao_blur_pipeline->SetResource(builder.GetTexture(m_gtao_factor_handle), "gtao_blur_in");
    m_gtao_blur_pipeline->SetResource(builder.GetTexture(m_depth_handle), "depth_tex");
    m_gtao_blur_pipeline->SetResource(builder.GetTexture(m_gbuffer0_handle), "normal_tex");
    m_gtao_blur_pipeline->SetResource(builder.GetTexture(m_gtao_blur_handle), "gtao_blur_out");
    cl->BindPipeline(m_gtao_blur_pipeline);
    cl->Dispatch(AlignUp<u32>(m_width, 8), AlignUp<u32>(m_height, 8), 1);
    cl->EndComputePass();
}
