#include "ssao.h"

#include <algorithm>

AOData::AOData(Backend::RHI *rhi, Camera *camera) noexcept : mRhi(rhi), m_camera(camera) {

    ao_cs = rhi->CreateShader(ShaderType::COMPUTE_SHADER, shader_dir / "ao.comp.hsl");
    ao_blur_cs = rhi->CreateShader(ShaderType::COMPUTE_SHADER, shader_dir / "ssao_blur.comp.hsl");

    // AO PASS
    {
        ao_pass = rhi->CreateComputePipeline(ComputePipelineCreateInfo{});
        ao_blur_pass = rhi->CreateComputePipeline({});
    }

    ssao_factor_image = rhi->CreateTexture(TextureCreateInfo{
        DescriptorType::DESCRIPTOR_TYPE_RW_TEXTURE, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS,
        TextureType::TEXTURE_TYPE_2D, TextureFormat::TEXTURE_FORMAT_RGBA8_UNORM, width, height, 1, false});

    ssao_noise_tex = rhi->CreateTexture(
        TextureCreateInfo{DescriptorType::DESCRIPTOR_TYPE_TEXTURE, ResourceState::RESOURCE_STATE_SHADER_RESOURCE,
                          TextureType::TEXTURE_TYPE_2D, TextureFormat::TEXTURE_FORMAT_RG32_SFLOAT, SSAO_NOISE_TEX_WIDTH,
                          SSAO_NOISE_TEX_HEIGHT, 1, false});
    hbao_noise_tex = rhi->CreateTexture(
        TextureCreateInfo{DescriptorType::DESCRIPTOR_TYPE_TEXTURE, ResourceState::RESOURCE_STATE_SHADER_RESOURCE,
                          TextureType::TEXTURE_TYPE_2D, TextureFormat::TEXTURE_FORMAT_RGBA32_SFLOAT,
                          HBAO_RAND_TEX_WIDTH, HBAO_RAND_TEX_HEIGHT, 1, false});
    ssao_blur_image = rhi->CreateTexture(TextureCreateInfo{
        DescriptorType::DESCRIPTOR_TYPE_RW_TEXTURE, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS,
        TextureType::TEXTURE_TYPE_2D, TextureFormat::TEXTURE_FORMAT_RGBA8_UNORM, width, height, 1, false});

    ssao_constants_buffer =
        rhi->CreateBuffer(BufferCreateInfo{DescriptorType::DESCRIPTOR_TYPE_CONSTANT_BUFFER,
                                           ResourceState::RESOURCE_STATE_SHADER_RESOURCE, sizeof(AOConstant)});

    ao_constansts.width = width;
    ao_constansts.height = height;

    std::uniform_real_distribution<float> rnd_dist(0.0, 1.0); // random floats between [0.0, 1.0]
    std::default_random_engine generator;

    for (unsigned int i = 0; i < SSAO_KERNEL_SIZE; ++i) {
        Math::float3 sample(rnd_dist(generator) * 2.0f - 1.0f, rnd_dist(generator) * 2.0f - 1.0f, rnd_dist(generator));
        sample.Normalize();
        sample *= rnd_dist(generator);
        float scale = float(i) / float(SSAO_KERNEL_SIZE);
        sample *= Lerp(0.1f, 1.0f, scale * scale);
        ao_constansts.kernels[i] = Math::float4(sample);
    }
    // ssao noise tex
    for (u32 i = 0; i < ssao_noise_tex_val.size(); i++) {
        ssao_noise_tex_val[i] = Math::float2(rnd_dist(generator) * 2.0f - 1.0f, rnd_dist(generator) * 2.0f - 1.0f);
    }
    ssao_noise_tex_data_desc.raw_data = {(char *)ssao_noise_tex_val.data(),
                                         (char *)ssao_noise_tex_val.data() + sizeof(ssao_noise_tex_val)};

    std::mt19937 rmt;

    float numDir = 8; // keep in sync to glsl

    std::array<Math::float4, HBAO_RAND_SIZE * HBAO_MAX_SAMPLES> hbao_rand;

    for (u32 i = 0; i < HBAO_RAND_SIZE * HBAO_MAX_SAMPLES; i++) {
        float Rand1 = static_cast<float>(rmt()) / 4294967296.0f;
        float Rand2 = static_cast<float>(rmt()) / 4294967296.0f;

        // Use random rotation angles in [0,2PI/NUM_DIRECTIONS)
        float Angle = 2.f * Math::_PI * Rand1 / numDir;
        hbao_rand[i].x = cosf(Angle);
        hbao_rand[i].y = sinf(Angle);
        hbao_rand[i].z = Rand2;
        hbao_rand[i].w = 0;
    }
    hbao_noise_tex_data_desc.raw_data = {(char *)hbao_rand.data(), (char *)hbao_rand.data() + sizeof(hbao_rand)};

    float meters2viewspace = 1.0f;
    float R = 2.0f * meters2viewspace;
    float projScale = (float)height / (tanf(m_camera->GetFov() * 0.5f) * 2.0f);
    ao_constansts.g_NegInvR2 = -1.0f / (R * R);
    ao_constansts.g_RadiusToScreen = R * 0.5f * projScale;
    ao_constansts.g_NDotVBias = std::min(std::max(0.0f, 0.1f), 1.0f);
    ao_constansts.ao_method = AO_METHOD::SSAO;
    ao_constansts.noise_scale_x = width / AOData::SSAO_NOISE_TEX_WIDTH;
    ao_constansts.noise_scale_y = height / AOData::SSAO_NOISE_TEX_HEIGHT;
    ao_constansts.ao_method = mAoMethod;
    ao_pass->SetComputeShader(ao_cs);
    ao_blur_pass->SetComputeShader(ao_blur_cs);
}

AOData::~AOData() noexcept {

    mRhi->DestroyShader(ao_cs);
    mRhi->DestroyPipeline(ao_pass);
    mRhi->DestroyShader(ao_blur_cs);
    mRhi->DestroyPipeline(ao_blur_pass);

    mRhi->DestroyTexture(ssao_noise_tex);
    mRhi->DestroyTexture(ssao_factor_image);
    mRhi->DestroyTexture(ssao_blur_image);
    mRhi->DestroyBuffer(ssao_constants_buffer);
    mRhi->DestroyTexture(hbao_noise_tex);
}

void AOData::SetAoMethod(AO_METHOD method) {
    if (mAoMethod == method) {
        return;
    }
    auto aoname = [](AO_METHOD m) -> std::string {
        switch (m) {
        case SSAO:
            return "SSAO";
        case HBAO:
            return "HBAO";
        default:
            return "INVLAID";
        }
    };
    LOG_INFO("switch to {}", aoname(method));
    mAoMethod = method;
    ao_constansts.ao_method = mAoMethod;
}
