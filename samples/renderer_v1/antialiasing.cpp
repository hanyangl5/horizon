#include "antialiasing.h"


AntialiasingData::AntialiasingData(Backend::RHI *rhi) noexcept : mRhi(rhi) {
    // PP PASS

    taa_cs = rhi->CreateShader(ShaderType::COMPUTE_SHADER, shader_dir / "taa.comp.hsl");
    taa_pass = rhi->CreateComputePipeline(ComputePipelineCreateInfo{});
    previous_color_texture = rhi->CreateTexture(TextureCreateInfo{
        DescriptorType::DESCRIPTOR_TYPE_RW_TEXTURE, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS,
        TextureType::TEXTURE_TYPE_2D, TextureFormat::TEXTURE_FORMAT_RGBA8_UNORM, width, height, 1, false});

    output_color_texture = rhi->CreateTexture(TextureCreateInfo{
        DescriptorType::DESCRIPTOR_TYPE_RW_TEXTURE, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS,
        TextureType::TEXTURE_TYPE_2D, TextureFormat::TEXTURE_FORMAT_RGBA8_UNORM, width, height, 1, false});
    taa_pass->SetComputeShader(taa_cs);

    taa_samples = {Math::float2(0.500000f, 0.333333f), Math::float2(0.250000f, 0.666667f), Math::float2(0.750000f, 0.111111f),
                   Math::float2(0.125000f, 0.444444f), Math::float2(0.625000f, 0.777778f), Math::float2(0.375000f, 0.222222f),
                   Math::float2(0.875000f, 0.555556f), Math::float2(0.062500f, 0.888889f), Math::float2(0.562500f, 0.037037f),
                   Math::float2(0.312500f, 0.370370f), Math::float2(0.812500f, 0.703704f), Math::float2(0.187500f, 0.148148f),
                   Math::float2(0.687500f, 0.481481f), Math::float2(0.437500f, 0.814815f), Math::float2(0.937500f, 0.259259f),
                   Math::float2(0.031250f, 0.592593f)};

    taa_prev_curr_offset_buffer =
        rhi->CreateBuffer(BufferCreateInfo{DescriptorType::DESCRIPTOR_TYPE_CONSTANT_BUFFER,
                                           ResourceState::RESOURCE_STATE_SHADER_RESOURCE, sizeof(TAAPrevCurrOffset)});
}

AntialiasingData::~AntialiasingData() noexcept {
    mRhi->DestroyBuffer(taa_prev_curr_offset_buffer);
    mRhi->DestroyTexture(previous_color_texture);
    mRhi->DestroyTexture(output_color_texture);
    mRhi->DestroyShader(taa_cs);
    mRhi->DestroyPipeline(taa_pass);
}
const Math::float2 &AntialiasingData::GetJitterOffset() noexcept {
    taa_sample_index %= TAA_SAMPLE_COUNT;
    return taa_samples[taa_sample_index++];
}
