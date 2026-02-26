#include "ssao_rdg_pass.h"
#include <random>

static constexpr u32 SSAO_KERNEL_SIZE = 32;
static constexpr u32 SSAO_NOISE_TEX_WIDTH = 4;
static constexpr u32 SSAO_NOISE_TEX_HEIGHT = 4;

SSAORDGPass::SSAORDGPass(RHI *rhi, Sampler *sampler, u32 width, u32 height)
    : RDGPass("SSAO Pass", rhi), m_rhi(rhi), m_sampler(sampler), m_width(width), m_height(height)
{
    m_ssao_cs = CreateShader(ShaderType::COMPUTE_SHADER, shader_dir / "ssao.comp.hlsl", "main");
    ComputePipelineCreateInfo create_info{};
    create_info.shader_program.SetShader(ShaderType::COMPUTE_SHADER, m_ssao_cs);
    m_ssao_pipeline = CreateComputePipeline(create_info);

    CreateResizableTexture(
        m_ssao_factor_image,
        TextureCreateInfo{DescriptorType::DESCRIPTOR_TYPE_RW_TEXTURE, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS,
                          TextureType::TEXTURE_TYPE_2D, TextureFormat::TEXTURE_FORMAT_R8_UNORM, m_width, m_height, 1,
                          false, 1, "ssao_factor_image"});

    m_ssao_noise_tex = rhi->CreateTexture(
        TextureCreateInfo{DescriptorType::DESCRIPTOR_TYPE_TEXTURE, ResourceState::RESOURCE_STATE_SHADER_RESOURCE,
                          TextureType::TEXTURE_TYPE_2D, TextureFormat::TEXTURE_FORMAT_RG32_SFLOAT, SSAO_NOISE_TEX_WIDTH,
                          SSAO_NOISE_TEX_HEIGHT, 1, false, 1, "ssao_noise_tex"});

    m_ssao_constants_buffer =
        m_rhi->CreateBuffer(BufferCreateInfo{DescriptorType::DESCRIPTOR_TYPE_CONSTANT_BUFFER,
                                             ResourceState::RESOURCE_STATE_SHADER_RESOURCE, sizeof(SSAOConstant)});

    // Initialize SSAO constants
    m_ssao_constants.width = m_width;
    m_ssao_constants.height = m_height;

    // Generate SSAO kernel
    std::uniform_real_distribution<float> rnd_dist(0.0, 1.0);
    std::default_random_engine generator;
    for (unsigned int i = 0; i < 32; ++i)
    {
        Math::float3 sample(rnd_dist(generator) * 2.0f - 1.0f, rnd_dist(generator) * 2.0f - 1.0f, rnd_dist(generator));
        sample.Normalize();
        sample *= rnd_dist(generator);
        float scale = float(i) / float(32);
        sample *= Horizon::Lerp(0.1f, 1.0f, scale * scale);
        m_ssao_constants.kernels[i] = Math::float4(sample);
    }

    // Generate SSAO noise texture data
    std::array<Math::float2, SSAO_NOISE_TEX_WIDTH * SSAO_NOISE_TEX_HEIGHT> ssao_noise_tex_val;
    std::uniform_real_distribution<float> rnd_dist1(0.0, 1.0);
    for (u32 i = 0; i < ssao_noise_tex_val.size(); i++)
    {
        ssao_noise_tex_val[i] = Math::float2(rnd_dist1(generator) * 2.0f - 1.0f, rnd_dist1(generator) * 2.0f - 1.0f);
    }
    char *begin = reinterpret_cast<char *>(&ssao_noise_tex_val[0]);
    char *end = reinterpret_cast<char *>(&ssao_noise_tex_val[ssao_noise_tex_val.size() - 1]);
    m_ssao_noise_tex_data_desc.raw_data = {begin, end};
    SetResizeCallback([this](u32 width, u32 height) {
        m_width = width;
        m_height = height;
        m_ssao_constants.width = width;
        m_ssao_constants.height = height;
    });
}

SSAORDGPass::~SSAORDGPass()
{
    DestroyShader(m_ssao_cs);
    DestroyPipeline(m_ssao_pipeline);
    m_rhi->DestroyTexture(m_ssao_factor_image);
    m_rhi->DestroyTexture(m_ssao_noise_tex);
    m_rhi->DestroyBuffer(m_ssao_constants_buffer);
}

void SSAORDGPass::ImportResources(Horizon::Backend::FrameGraph *frame_graph)
{
    m_ssao_factor_handle = frame_graph->ImportTexture("ssao_factor", m_ssao_factor_image);
    m_ssao_noise_handle = frame_graph->ImportTexture("ssao_noise", m_ssao_noise_tex);
}

void SSAORDGPass::SetInputHandles(Horizon::Backend::TextureHandle depth, Horizon::Backend::TextureHandle gbuffer0)
{
    m_depth_handle = depth;
    m_gbuffer0_handle = gbuffer0;
}

void SSAORDGPass::UpdateConstants(const void *data, u32 size)
{
    // This will be called externally to update constants
}

void SSAORDGPass::Setup(Horizon::Backend::FrameGraphBuilder &builder)
{
    builder.ReadTexture(m_depth_handle, ResourceState::RESOURCE_STATE_SHADER_RESOURCE);
    builder.ReadTexture(m_gbuffer0_handle, ResourceState::RESOURCE_STATE_SHADER_RESOURCE);
    builder.ReadTexture(m_ssao_noise_handle, ResourceState::RESOURCE_STATE_SHADER_RESOURCE);
    builder.WriteTexture(m_ssao_factor_handle, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);
}

void SSAORDGPass::Execute(CommandList *cl, Horizon::Backend::FrameGraphBuilder &builder)
{
    cl->BeginComputePass("SSAO Pass");
    m_ssao_pipeline->SetResource(builder.GetTexture(m_depth_handle), "depth_tex");
    m_ssao_pipeline->SetResource(builder.GetTexture(m_gbuffer0_handle), "normal_tex");
    m_ssao_pipeline->SetResource(m_sampler, "default_sampler");
    m_ssao_pipeline->SetResource(builder.GetTexture(m_ssao_factor_handle), "ao_factor_tex");
    m_ssao_pipeline->SetResource(m_ssao_constants_buffer, "SSAOConstant_cb");
    m_ssao_pipeline->SetResource(builder.GetTexture(m_ssao_noise_handle), "ssao_noise_tex");
    cl->BindPipeline(m_ssao_pipeline);
    cl->Dispatch(AlignUp<u32>(m_width, 8), AlignUp<u32>(m_height, 8), 1);
    cl->EndComputePass();
}
