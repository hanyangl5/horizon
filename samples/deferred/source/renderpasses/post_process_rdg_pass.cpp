#include "post_process_rdg_pass.h"

PostProcessRDGPass::PostProcessRDGPass(RHI *rhi, u32 width, u32 height)
    : RDGPass("Post Process Pass", rhi), m_rhi(rhi), m_width(width), m_height(height)
{
    m_post_process_cs = CreateShader(ShaderType::COMPUTE_SHADER, shader_dir / "post_process.comp.hlsl", "main");
    ComputePipelineCreateInfo create_info{};
    create_info.shader_program.SetShader(ShaderType::COMPUTE_SHADER, m_post_process_cs);
    m_post_process_pipeline = CreateComputePipeline(create_info);

    CreateResizableTexture(
        m_pp_color_image,
        TextureCreateInfo{DescriptorType::DESCRIPTOR_TYPE_RW_TEXTURE | DescriptorType::DESCRIPTOR_TYPE_TEXTURE,
                          ResourceState::RESOURCE_STATE_UNORDERED_ACCESS, TextureType::TEXTURE_TYPE_2D,
                          TextureFormat::TEXTURE_FORMAT_RGBA8_UNORM, m_width, m_height, 1, false});

    m_exposure_constants_buffer =
        rhi->CreateBuffer(BufferCreateInfo{DescriptorType::DESCRIPTOR_TYPE_CONSTANT_BUFFER,
                                           ResourceState::RESOURCE_STATE_SHADER_RESOURCE, sizeof(ExposureConstant)});
    SetResizeCallback([this](u32 width, u32 height) {
        m_width = width;
        m_height = height;
    });
}

PostProcessRDGPass::~PostProcessRDGPass()
{
    DestroyShader(m_post_process_cs);
    DestroyPipeline(m_post_process_pipeline);
    m_rhi->DestroyBuffer(m_exposure_constants_buffer);
    m_rhi->DestroyTexture(m_pp_color_image);
}

void PostProcessRDGPass::ImportResources(Horizon::Backend::FrameGraph *frame_graph)
{
    m_pp_color_handle = frame_graph->ImportTexture("pp_color", m_pp_color_image);
}

void PostProcessRDGPass::SetInputHandles(Horizon::Backend::TextureHandle shading_color,
                                         Horizon::Backend::BufferHandle adapted_luminance)
{
    m_shading_color_handle = shading_color;
    m_adapted_luminance_handle = adapted_luminance;
}

void PostProcessRDGPass::UpdateConstants(const void *data, u32 size)
{
    // This will be called externally to update constants
}

void PostProcessRDGPass::Setup(Horizon::Backend::FrameGraphBuilder &builder)
{
    builder.ReadTexture(m_shading_color_handle, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);
    builder.ReadBuffer(m_adapted_luminance_handle, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);
    builder.WriteTexture(m_pp_color_handle, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);
}

void PostProcessRDGPass::Execute(CommandList *cl, Horizon::Backend::FrameGraphBuilder &builder)
{
    cl->BeginComputePass("Post Process Pass");
    m_post_process_pipeline->SetResource(builder.GetTexture(m_shading_color_handle), "color_image");
    m_post_process_pipeline->SetResource(builder.GetTexture(m_pp_color_handle), "out_color_image");
    m_post_process_pipeline->SetResource(builder.GetBuffer(m_adapted_luminance_handle), "adaptedLuminance");
    cl->BindPipeline(m_post_process_pipeline);
    cl->Dispatch(AlignUp<u32>(m_width, 8), AlignUp<u32>(m_height, 8), 1);
    cl->EndComputePass();
}
