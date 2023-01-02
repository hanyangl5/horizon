#include "ssao.h"

SSAOData::SSAOData(Backend::RHI *rhi) noexcept {

    ssao_cs = rhi->CreateShader(ShaderType::COMPUTE_SHADER, 0, asset_path / "shaders/ssao.comp.hsl");
    ssao_blur_cs = rhi->CreateShader(ShaderType::COMPUTE_SHADER, 0, asset_path / "shaders/ssao_blur.comp.hsl");

    // AO PASS
    {
        ssao_pass = rhi->CreateComputePipeline(ComputePipelineCreateInfo{});
        ssao_blur_pass = rhi->CreateComputePipeline({});
    }

    ssao_factor_texture = rhi->CreateTexture(TextureCreateInfo{
        DescriptorType::DESCRIPTOR_TYPE_RW_TEXTURE, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS,
        TextureType::TEXTURE_TYPE_2D, TextureFormat::TEXTURE_FORMAT_RGBA8_UNORM, _width, _height, 1, false});

    ssao_noise_tex = rhi->CreateTexture(
        TextureCreateInfo{DescriptorType::DESCRIPTOR_TYPE_TEXTURE, ResourceState::RESOURCE_STATE_SHADER_RESOURCE,
                          TextureType::TEXTURE_TYPE_2D, TextureFormat::TEXTURE_FORMAT_RG32_SFLOAT, SSAO_NOISE_TEX_WIDTH,
                          SSAO_NOISE_TEX_HEIGHT, 1, false});

    ssao_blur_texture = rhi->CreateTexture(TextureCreateInfo{
        DescriptorType::DESCRIPTOR_TYPE_RW_TEXTURE, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS,
        TextureType::TEXTURE_TYPE_2D, TextureFormat::TEXTURE_FORMAT_RGBA8_UNORM, _width, _height, 1, false});

    ssao_constants_buffer =
        rhi->CreateBuffer(BufferCreateInfo{DescriptorType::DESCRIPTOR_TYPE_CONSTANT_BUFFER,
                                           ResourceState::RESOURCE_STATE_SHADER_RESOURCE, sizeof(SSAOConstant)});

    std::uniform_real_distribution<float> rnd_dist(0.0, 1.0); // random floats between [0.0, 1.0]
    std::default_random_engine generator;

    for (unsigned int i = 0; i < SSAO_KERNEL_SIZE; ++i) {
        Math::float3 sample(rnd_dist(generator) * 2.0 - 1.0, rnd_dist(generator) * 2.0 - 1.0, rnd_dist(generator));
        sample.Normalize();
        sample *= rnd_dist(generator);
        float scale = float(i) / float(SSAO_KERNEL_SIZE);
        sample *= Lerp(0.1f, 1.0f, scale * scale);
        ssao_constansts.kernels[i] = Math::float4(sample);
    }
    // ssao noise tex
    for (u32 i = 0; i < ssao_noise_tex_val.size(); i++) {
        ssao_noise_tex_val[i] = Math::float2(rnd_dist(generator) * 2.0f - 1.0f, rnd_dist(generator) * 2.0f - 1.0f);
    }
    char *begin = reinterpret_cast<char *>(&ssao_noise_tex_val[0]);
    char *end = reinterpret_cast<char *>(&ssao_noise_tex_val[ssao_noise_tex_val.size() - 1]);
    ssao_noise_tex_data_desc.raw_data = {begin, end};

    ssao_pass->SetComputeShader(ssao_cs);
    ssao_blur_pass->SetComputeShader(ssao_blur_cs);
}

SSAOData::SSAOData() noexcept {}
