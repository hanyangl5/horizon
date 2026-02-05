#include "ssao_rdg_pass.h"
#include <random>

static constexpr u32 SSAO_KERNEL_SIZE = 32;
static constexpr u32 SSAO_NOISE_TEX_WIDTH = 4;
static constexpr u32 SSAO_NOISE_TEX_HEIGHT = 4;

SSAORDGPass::SSAORDGPass(RHI *rhi, Sampler *sampler)
    : RDGPass("SSAO Pass", rhi), m_rhi(rhi), m_sampler(sampler)
{
    m_ssao_cs = CreateShader(ShaderType::COMPUTE_SHADER, shader_dir / "ssao.comp.hlsl", "main");
    m_ssao_pipeline = CreateComputePipeline(ComputePipelineCreateInfo{});
    m_ssao_pipeline->SetComputeShader(m_ssao_cs);

    m_ssao_factor_image = rhi->CreateTexture(
        TextureCreateInfo{DescriptorType::DESCRIPTOR_TYPE_RW_TEXTURE, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS,
                          TextureType::TEXTURE_TYPE_2D, TextureFormat::TEXTURE_FORMAT_R8_UNORM, width, height, 1, false,
                          1, "ssao_factor_image"});

    m_ssao_noise_tex = rhi->CreateTexture(
        TextureCreateInfo{DescriptorType::DESCRIPTOR_TYPE_TEXTURE, ResourceState::RESOURCE_STATE_SHADER_RESOURCE,
                          TextureType::TEXTURE_TYPE_2D, TextureFormat::TEXTURE_FORMAT_RG32_SFLOAT, SSAO_NOISE_TEX_WIDTH,
                          SSAO_NOISE_TEX_HEIGHT, 1, false, 1, "ssao_noise_tex"});

    m_ssao_constants_buffer =
        m_rhi->CreateBuffer(BufferCreateInfo{DescriptorType::DESCRIPTOR_TYPE_CONSTANT_BUFFER,
                                             ResourceState::RESOURCE_STATE_SHADER_RESOURCE, 256}); // Size will be set properly
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
    cl->Dispatch(AlignUp<u32>(width, 8), AlignUp<u32>(height, 8), 1);
    cl->EndComputePass();
}
