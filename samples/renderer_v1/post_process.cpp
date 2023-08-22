#include "post_process.h"

AutoExposure::AutoExposure(Backend::RHI *rhi) noexcept : mRhi(rhi) {
    // PP PASS
    luminance_histogram_cs = rhi->CreateShader(ShaderType::COMPUTE_SHADER, shader_dir / "luminance_histogram.comp.hsl");
    luminance_average_cs = rhi->CreateShader(ShaderType::COMPUTE_SHADER, shader_dir / "luminance_average.comp.hsl");
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

AutoExposure::~AutoExposure() noexcept {

    mRhi->DestroyPipeline(luminance_histogram_pass);
    mRhi->DestroyPipeline(luminance_average_pass);

    mRhi->DestroyShader(luminance_histogram_cs);
    mRhi->DestroyShader(luminance_average_cs);

    mRhi->DestroyBuffer(histogram_buffer);
    mRhi->DestroyBuffer(luminance_histogram_constants_buffer);
    mRhi->DestroyBuffer(adapted_muminance_buffer);
}
PostProcessData::PostProcessData(Backend::RHI *rhi) noexcept : mRhi(rhi) {
    // PP PASS
    post_process_cs = rhi->CreateShader(ShaderType::COMPUTE_SHADER, shader_dir / "post_process.comp.hsl");
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

PostProcessData::~PostProcessData() noexcept {

    mRhi->DestroyPipeline(post_process_pass);
    mRhi->DestroyShader(post_process_cs);
    mRhi->DestroyBuffer(exposure_constants_buffer);
    mRhi->DestroyTexture(pp_color_image);
}