#include "luminance_histogram_rdg_pass.h"

LuminanceHistogramRDGPass::LuminanceHistogramRDGPass(RHI *rhi, u32 width, u32 height)
    : RDGPass("Luminance Histogram Pass", rhi), m_rhi(rhi), m_width(width), m_height(height)
{
    m_luminance_histogram_cs =
        CreateShader(ShaderType::COMPUTE_SHADER, shader_dir / "luminance_histogram.comp.hlsl", "main");
    ComputePipelineCreateInfo create_info{};
    create_info.shader_program.SetShader(ShaderType::COMPUTE_SHADER, m_luminance_histogram_cs);
    m_luminance_histogram_pipeline = CreateComputePipeline(create_info);

    m_luminance_histogram_constants_buffer = rhi->CreateBuffer(
        BufferCreateInfo{DescriptorType::DESCRIPTOR_TYPE_CONSTANT_BUFFER, ResourceState::RESOURCE_STATE_SHADER_RESOURCE,
                         sizeof(LuminanceHistogramConstants)});

    m_histogram_buffer = rhi->CreateBuffer(BufferCreateInfo{
        DescriptorType::DESCRIPTOR_TYPE_RW_BUFFER, ResourceState::RESOURCE_STATE_SHADER_RESOURCE, 256 * sizeof(u32)});

    m_adapted_luminance_buffer = rhi->CreateBuffer(
        BufferCreateInfo{DescriptorType::DESCRIPTOR_TYPE_RW_BUFFER, ResourceState::RESOURCE_STATE_SHADER_RESOURCE, 4});
    SetResizeCallback([this](u32 width, u32 height) {
        m_width = width;
        m_height = height;
        m_luminance_histogram_constants.width = width;
        m_luminance_histogram_constants.height = height;
        m_luminance_histogram_constants.pixelCount = width * height;
    });
}

LuminanceHistogramRDGPass::~LuminanceHistogramRDGPass()
{
    DestroyShader(m_luminance_histogram_cs);
    DestroyPipeline(m_luminance_histogram_pipeline);
    m_rhi->DestroyBuffer(m_luminance_histogram_constants_buffer);
    m_rhi->DestroyBuffer(m_histogram_buffer);
    m_rhi->DestroyBuffer(m_adapted_luminance_buffer);
}

void LuminanceHistogramRDGPass::ImportResources(Horizon::Backend::FrameGraph *frame_graph)
{
    m_histogram_buffer_handle = frame_graph->ImportBuffer("histogram_buffer", m_histogram_buffer);
    m_adapted_luminance_handle = frame_graph->ImportBuffer("adapted_luminance", m_adapted_luminance_buffer);
}

void LuminanceHistogramRDGPass::SetInputHandle(Horizon::Backend::TextureHandle shading_color)
{
    m_shading_color_handle = shading_color;
}

void LuminanceHistogramRDGPass::UpdateConstants(const void *data, u32 size)
{
    // This will be called externally to update constants
}

void LuminanceHistogramRDGPass::Setup(Horizon::Backend::FrameGraphBuilder &builder)
{
    builder.ReadTexture(m_shading_color_handle, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);
    builder.WriteBuffer(m_histogram_buffer_handle, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);
    // adapted_luminance is only written by LuminanceAverageRDGPass, not here
}

void LuminanceHistogramRDGPass::Execute(CommandList *cl, Horizon::Backend::FrameGraphBuilder &builder)
{
    cl->BeginComputePass("Luminance Histogram Pass");
    m_luminance_histogram_pipeline->SetResource(builder.GetTexture(m_shading_color_handle), "color_image");
    m_luminance_histogram_pipeline->SetResource(m_luminance_histogram_constants_buffer,
                                                "LuminanceHistogramConstants_cb");
    m_luminance_histogram_pipeline->SetResource(builder.GetBuffer(m_histogram_buffer_handle), "histogram");
    // adaptedLuminance is not used in this pass - it's only written by LuminanceAverageRDGPass
    cl->BindPipeline(m_luminance_histogram_pipeline);
    cl->Dispatch(AlignUp<u32>(m_width, 16), AlignUp<u32>(m_height, 16), 1);
    cl->EndComputePass();
}
