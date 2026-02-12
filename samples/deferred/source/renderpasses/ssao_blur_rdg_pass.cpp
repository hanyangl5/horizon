#include "ssao_blur_rdg_pass.h"

SSAOBlurRDGPass::SSAOBlurRDGPass(RHI *rhi, u32 width, u32 height) : RDGPass("SSAO Blur Pass", rhi), m_rhi(rhi), m_width(width), m_height(height)
{
    m_ssao_blur_cs = CreateShader(ShaderType::COMPUTE_SHADER, shader_dir / "ssao_blur.comp.hlsl", "main");
    ComputePipelineCreateInfo create_info{};
    create_info.shader_program.SetShader(ShaderType::COMPUTE_SHADER, m_ssao_blur_cs);
    m_ssao_blur_pipeline = CreateComputePipeline(create_info);

    m_ssao_blur_image = rhi->CreateTexture(
        TextureCreateInfo{DescriptorType::DESCRIPTOR_TYPE_RW_TEXTURE | DescriptorType::DESCRIPTOR_TYPE_TEXTURE,
                          ResourceState::RESOURCE_STATE_UNORDERED_ACCESS, TextureType::TEXTURE_TYPE_2D,
                          TextureFormat::TEXTURE_FORMAT_R8_UNORM, m_width, m_height, 1, false, 1, "ssao_blur_image"});
}

SSAOBlurRDGPass::~SSAOBlurRDGPass()
{
    DestroyShader(m_ssao_blur_cs);
    DestroyPipeline(m_ssao_blur_pipeline);
    m_rhi->DestroyTexture(m_ssao_blur_image);
}

void SSAOBlurRDGPass::ImportResources(Horizon::Backend::FrameGraph *frame_graph)
{
    m_ssao_blur_handle = frame_graph->ImportTexture("ssao_blur", m_ssao_blur_image);
}

void SSAOBlurRDGPass::SetInputHandle(Horizon::Backend::TextureHandle ssao_factor)
{
    m_ssao_factor_handle = ssao_factor;
}

void SSAOBlurRDGPass::Setup(Horizon::Backend::FrameGraphBuilder &builder)
{
    builder.ReadTexture(m_ssao_factor_handle, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);
    builder.WriteTexture(m_ssao_blur_handle, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);
}

void SSAOBlurRDGPass::Execute(CommandList *cl, Horizon::Backend::FrameGraphBuilder &builder)
{
    cl->BeginComputePass("SSAO Blur Pass");
    m_ssao_blur_pipeline->SetResource(builder.GetTexture(m_ssao_factor_handle), "ssao_blur_in");
    m_ssao_blur_pipeline->SetResource(builder.GetTexture(m_ssao_blur_handle), "ssao_blur_out");
    cl->BindPipeline(m_ssao_blur_pipeline);
    cl->Dispatch(AlignUp<u32>(m_width, 8), AlignUp<u32>(m_height, 8), 1);
    cl->EndComputePass();
}
