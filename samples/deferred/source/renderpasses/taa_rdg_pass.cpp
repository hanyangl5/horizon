#include "taa_rdg_pass.h"

TAARDGPass::TAARDGPass(RHI *rhi) : RDGPass("TAA Pass", rhi), m_rhi(rhi)
{
    m_taa_cs = CreateShader(ShaderType::COMPUTE_SHADER, shader_dir / "taa.comp.hlsl", "main");
    ComputePipelineCreateInfo create_info{};
    create_info.shader_program.SetShader(ShaderType::COMPUTE_SHADER, m_taa_cs);
    m_taa_pipeline = CreateComputePipeline(create_info);

    m_previous_color_texture = rhi->CreateTexture(TextureCreateInfo{
        DescriptorType::DESCRIPTOR_TYPE_RW_TEXTURE, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS,
        TextureType::TEXTURE_TYPE_2D, TextureFormat::TEXTURE_FORMAT_RGBA8_UNORM, width, height, 1, false});

    m_output_color_texture = rhi->CreateTexture(TextureCreateInfo{
        DescriptorType::DESCRIPTOR_TYPE_RW_TEXTURE, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS,
        TextureType::TEXTURE_TYPE_2D, TextureFormat::TEXTURE_FORMAT_RGBA8_UNORM, width, height, 1, false});

    m_taa_prev_curr_offset_buffer =
        rhi->CreateBuffer(BufferCreateInfo{DescriptorType::DESCRIPTOR_TYPE_CONSTANT_BUFFER,
                                           ResourceState::RESOURCE_STATE_SHADER_RESOURCE, sizeof(TAAPrevCurrOffset)});

    m_taa_samples = {
        Math::float2(0.500000f, 0.333333f), Math::float2(0.250000f, 0.666667f), Math::float2(0.750000f, 0.111111f),
        Math::float2(0.125000f, 0.444444f), Math::float2(0.625000f, 0.777778f), Math::float2(0.375000f, 0.222222f),
        Math::float2(0.875000f, 0.555556f), Math::float2(0.062500f, 0.888889f), Math::float2(0.562500f, 0.037037f),
        Math::float2(0.312500f, 0.370370f), Math::float2(0.812500f, 0.703704f), Math::float2(0.187500f, 0.148148f),
        Math::float2(0.687500f, 0.481481f), Math::float2(0.437500f, 0.814815f), Math::float2(0.937500f, 0.259259f),
        Math::float2(0.031250f, 0.592593f)};
}

TAARDGPass::~TAARDGPass()
{
    DestroyShader(m_taa_cs);
    DestroyPipeline(m_taa_pipeline);
    m_rhi->DestroyBuffer(m_taa_prev_curr_offset_buffer);
    m_rhi->DestroyTexture(m_previous_color_texture);
    m_rhi->DestroyTexture(m_output_color_texture);
}

void TAARDGPass::ImportResources(Horizon::Backend::FrameGraph *frame_graph)
{
    m_output_color_handle = frame_graph->ImportTexture("output_color", m_output_color_texture);
    m_previous_color_handle = frame_graph->ImportTexture("previous_color", m_previous_color_texture);
}

void TAARDGPass::SetInputHandles(Horizon::Backend::TextureHandle previous_color,
                                 Horizon::Backend::TextureHandle pp_color, Horizon::Backend::TextureHandle gbuffer4)
{
    m_previous_color_handle = previous_color;
    m_pp_color_handle = pp_color;
    m_gbuffer4_handle = gbuffer4;
}

const Math::float2 &TAARDGPass::GetJitterOffset() noexcept
{
    m_taa_sample_index %= TAA_SAMPLE_COUNT;
    return m_taa_samples[m_taa_sample_index++];
}

void TAARDGPass::UpdateTAAPrevCurrOffset(const void *data, u32 size)
{
    // This will be called externally to update constants
}

void TAARDGPass::Setup(Horizon::Backend::FrameGraphBuilder &builder)
{
    builder.ReadTexture(m_previous_color_handle, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);
    builder.ReadTexture(m_pp_color_handle, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);
    builder.ReadTexture(m_gbuffer4_handle, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);
    builder.WriteTexture(m_output_color_handle, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);
}

void TAARDGPass::Execute(CommandList *cl, Horizon::Backend::FrameGraphBuilder &builder)
{
    cl->BeginComputePass("TAA Pass");
    m_taa_pipeline->SetResource(builder.GetTexture(m_previous_color_handle), "prev_color_tex");
    m_taa_pipeline->SetResource(builder.GetTexture(m_pp_color_handle), "curr_color_tex");
    m_taa_pipeline->SetResource(builder.GetTexture(m_gbuffer4_handle), "mv_tex");
    m_taa_pipeline->SetResource(builder.GetTexture(m_output_color_handle), "out_color_tex");
    cl->BindPipeline(m_taa_pipeline);
    cl->Dispatch(AlignUp<u32>(width, 8), AlignUp<u32>(height, 8), 1);
    cl->EndComputePass();
}
