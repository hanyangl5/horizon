#include "luminance_average_rdg_pass.h"
#include "luminance_histogram_rdg_pass.h"

LuminanceAverageRDGPass::LuminanceAverageRDGPass(RHI *rhi) : RDGPass("Luminance Average Pass", rhi), m_rhi(rhi)
{
    m_luminance_average_cs =
        CreateShader(ShaderType::COMPUTE_SHADER, shader_dir / "luminance_average.comp.hlsl", "main");
    m_luminance_average_pipeline = CreateComputePipeline(ComputePipelineCreateInfo{});
    m_luminance_average_pipeline->SetComputeShader(m_luminance_average_cs);
}

LuminanceAverageRDGPass::~LuminanceAverageRDGPass()
{
    DestroyShader(m_luminance_average_cs);
    DestroyPipeline(m_luminance_average_pipeline);
}

void LuminanceAverageRDGPass::ImportResources(Horizon::Backend::FrameGraph *frame_graph)
{
    // Buffers are imported by LuminanceHistogramRDGPass, we just need to reference them
}

void LuminanceAverageRDGPass::SetInputHandles(Horizon::Backend::BufferHandle histogram_buffer,
                                              Horizon::Backend::BufferHandle adapted_luminance)
{
    m_histogram_buffer_handle = histogram_buffer;
    m_adapted_luminance_handle = adapted_luminance;
}

void LuminanceAverageRDGPass::SetLuminanceHistogramPass(LuminanceHistogramRDGPass *luminance_histogram_pass)
{
    m_luminance_histogram_pass = luminance_histogram_pass;
}

void LuminanceAverageRDGPass::Setup(Horizon::Backend::FrameGraphBuilder &builder)
{
    builder.ReadBuffer(m_histogram_buffer_handle, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);
    builder.WriteBuffer(m_adapted_luminance_handle, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);
}

void LuminanceAverageRDGPass::Execute(CommandList *cl, Horizon::Backend::FrameGraphBuilder &builder)
{
    cl->BeginComputePass("Luminance Average Pass");
    if (m_luminance_histogram_pass)
    {
        m_luminance_average_pipeline->SetResource(m_luminance_histogram_pass->GetConstantsBuffer(),
                                                  "LuminanceHistogramConstants_cb");
    }
    m_luminance_average_pipeline->SetResource(builder.GetBuffer(m_histogram_buffer_handle), "histogram");
    m_luminance_average_pipeline->SetResource(builder.GetBuffer(m_adapted_luminance_handle), "adaptedLuminance");
    cl->BindPipeline(m_luminance_average_pipeline);
    cl->Dispatch(1, 1, 1);
    cl->EndComputePass();
}
