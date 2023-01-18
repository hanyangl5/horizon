#include "antialiasing.h"

TAAData::TAAData(Backend::RHI *rhi) noexcept {
    // PP PASS
    taa_cs = rhi->CreateShader(ShaderType::COMPUTE_SHADER, 0, asset_path / "shaders/taa.comp.hsl");
    taa_pass = rhi->CreateComputePipeline(ComputePipelineCreateInfo{});
    previous_color_texture = rhi->CreateTexture(TextureCreateInfo{
        DescriptorType::DESCRIPTOR_TYPE_RW_TEXTURE, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS,
        TextureType::TEXTURE_TYPE_2D, TextureFormat::TEXTURE_FORMAT_RGBA8_UNORM, _width, _height, 1, false});

    output_color_texture = rhi->CreateTexture(TextureCreateInfo{
        DescriptorType::DESCRIPTOR_TYPE_RW_TEXTURE, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS,
        TextureType::TEXTURE_TYPE_2D, TextureFormat::TEXTURE_FORMAT_RGBA8_UNORM, _width, _height, 1, false});
    taa_pass->SetComputeShader(taa_cs);

    taa_samples = {math::Vector2f(0.500000, 0.333333), math::Vector2f(0.250000, 0.666667), math::Vector2f(0.750000, 0.111111),
                  math::Vector2f(0.125000, 0.444444), math::Vector2f(0.625000, 0.777778), math::Vector2f(0.375000, 0.222222),
                  math::Vector2f(0.875000, 0.555556), math::Vector2f(0.062500, 0.888889), math::Vector2f(0.562500, 0.037037),
                  math::Vector2f(0.312500, 0.370370), math::Vector2f(0.812500, 0.703704), math::Vector2f(0.187500, 0.148148),
                  math::Vector2f(0.687500, 0.481481), math::Vector2f(0.437500, 0.814815), math::Vector2f(0.937500, 0.259259),
                  math::Vector2f(0.031250, 0.592593)};

    taa_prev_curr_offset_buffer =
        rhi->CreateBuffer(BufferCreateInfo{DescriptorType::DESCRIPTOR_TYPE_CONSTANT_BUFFER,
                                           ResourceState::RESOURCE_STATE_SHADER_RESOURCE, sizeof(TAAPrevCurrOffset)});
}

const math::Vector2f& TAAData::GetJitterOffset() noexcept {
    taa_sample_index %= TAA_SAMPLE_COUNT;
    return taa_samples[taa_sample_index++];
    
}
