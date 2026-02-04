#include "post_process.h"

AutoExposure::AutoExposure(Backend::RHI *rhi) noexcept : mRhi(rhi)
{
    // PP PASS
    luminance_histogram_cs =
        rhi->CreateShader(ShaderType::COMPUTE_SHADER, shader_dir / "luminance_histogram.comp.hlsl", "main");
    luminance_average_cs =
        rhi->CreateShader(ShaderType::COMPUTE_SHADER, shader_dir / "luminance_average.comp.hlsl", "main");
    luminance_histogram_pass = rhi->CreateComputePipeline(ComputePipelineCreateInfo{});
    luminance_average_pass = rhi->CreateComputePipeline(ComputePipelineCreateInfo{});

    luminance_histogram_constants_buffer = rhi->CreateBuffer(
        BufferCreateInfo{DescriptorType::DESCRIPTOR_TYPE_CONSTANT_BUFFER, ResourceState::RESOURCE_STATE_SHADER_RESOURCE,
                         sizeof(luminance_histogram_constants)});

    histogram_buffer = rhi->CreateBuffer(BufferCreateInfo{
        DescriptorType::DESCRIPTOR_TYPE_RW_BUFFER, ResourceState::RESOURCE_STATE_SHADER_RESOURCE, 256 * sizeof(u32)});

    adapted_muminance_buffer = rhi->CreateBuffer(
        BufferCreateInfo{DescriptorType::DESCRIPTOR_TYPE_RW_BUFFER, ResourceState::RESOURCE_STATE_SHADER_RESOURCE, 4});

    luminance_histogram_pass->SetComputeShader(luminance_histogram_cs);
    luminance_average_pass->SetComputeShader(luminance_average_cs);
}

AutoExposure::~AutoExposure() noexcept
{

    mRhi->DestroyPipeline(luminance_histogram_pass);
    mRhi->DestroyPipeline(luminance_average_pass);

    mRhi->DestroyShader(luminance_histogram_cs);
    mRhi->DestroyShader(luminance_average_cs);

    mRhi->DestroyBuffer(histogram_buffer);
    mRhi->DestroyBuffer(luminance_histogram_constants_buffer);
    mRhi->DestroyBuffer(adapted_muminance_buffer);
}

void AutoExposure::ImportResources(Horizon::Backend::FrameGraph *frame_graph,
                                    Horizon::Backend::BufferHandle &histogram_buffer_handle,
                                    Horizon::Backend::BufferHandle &adapted_luminance_handle)
{
    histogram_buffer_handle = frame_graph->ImportBuffer("histogram_buffer", histogram_buffer);
    adapted_luminance_handle = frame_graph->ImportBuffer("adapted_luminance", adapted_muminance_buffer);
}

void AutoExposure::SetupLuminanceHistogramPass(Horizon::Backend::FrameGraphBuilder &builder,
                                                 Horizon::Backend::TextureHandle shading_color_handle,
                                                 Horizon::Backend::BufferHandle histogram_buffer_handle,
                                                 Horizon::Backend::BufferHandle adapted_luminance_handle)
{
    builder.ReadTexture(shading_color_handle, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);
    builder.WriteBuffer(histogram_buffer_handle, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);
    builder.WriteBuffer(adapted_luminance_handle, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);
}

void AutoExposure::ExecuteLuminanceHistogramPass(CommandList *cl, Horizon::Backend::FrameGraphBuilder &builder,
                                                  Horizon::Backend::TextureHandle shading_color_handle,
                                                  Horizon::Backend::BufferHandle histogram_buffer_handle,
                                                  Horizon::Backend::BufferHandle adapted_luminance_handle)
{
    cl->BeginComputePass("Luminance Histogram Pass");
    luminance_histogram_pass->SetResource(builder.GetTexture(shading_color_handle), "color_image");
    luminance_histogram_pass->SetResource(luminance_histogram_constants_buffer, "LuminanceHistogramConstants_cb");
    luminance_histogram_pass->SetResource(builder.GetBuffer(histogram_buffer_handle), "histogram");
    luminance_histogram_pass->SetResource(builder.GetBuffer(adapted_luminance_handle), "adaptedLuminance");
    cl->BindPipeline(luminance_histogram_pass);
    cl->Dispatch(AlignUp<u32>(width, 16), AlignUp<u32>(height, 16), 1);
    cl->EndComputePass();
}

void AutoExposure::SetupLuminanceAveragePass(Horizon::Backend::FrameGraphBuilder &builder,
                                             Horizon::Backend::BufferHandle histogram_buffer_handle,
                                             Horizon::Backend::BufferHandle adapted_luminance_handle)
{
    builder.ReadBuffer(histogram_buffer_handle, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);
    builder.WriteBuffer(adapted_luminance_handle, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);
}

void AutoExposure::ExecuteLuminanceAveragePass(CommandList *cl, Horizon::Backend::FrameGraphBuilder &builder,
                                                Horizon::Backend::BufferHandle histogram_buffer_handle,
                                                Horizon::Backend::BufferHandle adapted_luminance_handle)
{
    cl->BeginComputePass("Luminance Average Pass");
    luminance_average_pass->SetResource(luminance_histogram_constants_buffer, "LuminanceHistogramConstants_cb");
    luminance_average_pass->SetResource(builder.GetBuffer(histogram_buffer_handle), "histogram");
    luminance_average_pass->SetResource(builder.GetBuffer(adapted_luminance_handle), "adaptedLuminance");
    cl->BindPipeline(luminance_average_pass);
    cl->Dispatch(1, 1, 1);
    cl->EndComputePass();
}
PostProcessingPass::PostProcessingPass(Backend::RHI *rhi) noexcept : mRhi(rhi)
{
    // PP PASS
    post_process_cs = rhi->CreateShader(ShaderType::COMPUTE_SHADER, shader_dir / "post_process.comp.hlsl", "main");
    post_process_pass = rhi->CreateComputePipeline(ComputePipelineCreateInfo{});
    pp_color_image = rhi->CreateTexture(TextureCreateInfo{
        DescriptorType::DESCRIPTOR_TYPE_RW_TEXTURE, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS,
        TextureType::TEXTURE_TYPE_2D, TextureFormat::TEXTURE_FORMAT_RGBA8_UNORM, width, height, 1, false});
    exposure_constants_buffer =
        rhi->CreateBuffer(BufferCreateInfo{DescriptorType::DESCRIPTOR_TYPE_CONSTANT_BUFFER,
                                           ResourceState::RESOURCE_STATE_SHADER_RESOURCE, sizeof(ExposureConstant)});
    post_process_pass->SetComputeShader(post_process_cs);

    auto_exposure_pass = std::make_unique<AutoExposure>(rhi);
}

PostProcessingPass::~PostProcessingPass() noexcept
{

    mRhi->DestroyPipeline(post_process_pass);
    mRhi->DestroyShader(post_process_cs);
    mRhi->DestroyBuffer(exposure_constants_buffer);
    mRhi->DestroyTexture(pp_color_image);
}

void PostProcessingPass::ImportResources(Horizon::Backend::FrameGraph *frame_graph,
                                         Horizon::Backend::TextureHandle &pp_color_handle)
{
    pp_color_handle = frame_graph->ImportTexture("pp_color", pp_color_image);
}

void PostProcessingPass::SetupPostProcessPass(Horizon::Backend::FrameGraphBuilder &builder,
                                              Horizon::Backend::TextureHandle shading_color_handle,
                                              Horizon::Backend::TextureHandle pp_color_handle,
                                              Horizon::Backend::BufferHandle adapted_luminance_handle)
{
    builder.ReadTexture(shading_color_handle, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);
    builder.ReadBuffer(adapted_luminance_handle, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);
    builder.WriteTexture(pp_color_handle, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);
}

void PostProcessingPass::ExecutePostProcessPass(CommandList *cl, Horizon::Backend::FrameGraphBuilder &builder,
                                                 Horizon::Backend::TextureHandle shading_color_handle,
                                                 Horizon::Backend::TextureHandle pp_color_handle,
                                                 Horizon::Backend::BufferHandle adapted_luminance_handle)
{
    cl->BeginComputePass("Post Process Pass");
    post_process_pass->SetResource(builder.GetTexture(shading_color_handle), "color_image");
    post_process_pass->SetResource(builder.GetTexture(pp_color_handle), "out_color_image");
    post_process_pass->SetResource(builder.GetBuffer(adapted_luminance_handle), "adaptedLuminance");
    cl->BindPipeline(post_process_pass);
    cl->Dispatch(AlignUp<u32>(width, 8), AlignUp<u32>(height, 8), 1);
    cl->EndComputePass();
}