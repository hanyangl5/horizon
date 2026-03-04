#include "gtao_rdg_pass.h"

GTAORDGPass::GTAORDGPass(RHI *rhi, Sampler *sampler, u32 width, u32 height)
    : RDGPass("GTAO Pass", rhi), m_rhi(rhi), m_sampler(sampler), m_width(width), m_height(height)
{
    m_gtao_cs = CreateShader(ShaderType::COMPUTE_SHADER, shader_dir / "gtao.comp.hlsl", "main");
    ComputePipelineCreateInfo create_info{};
    create_info.shader_program.SetShader(ShaderType::COMPUTE_SHADER, m_gtao_cs);
    m_gtao_pipeline = CreateComputePipeline(create_info);

    CreateResizableTexture(
        m_gtao_factor_image,
        TextureCreateInfo{DescriptorType::DESCRIPTOR_TYPE_RW_TEXTURE | DescriptorType::DESCRIPTOR_TYPE_TEXTURE,
                          ResourceState::RESOURCE_STATE_UNORDERED_ACCESS, TextureType::TEXTURE_TYPE_2D,
                          TextureFormat::TEXTURE_FORMAT_R8_UNORM, m_width, m_height, 1, false, 1, "gtao_factor_image"});

    m_gtao_constants_buffer =
        m_rhi->CreateBuffer(BufferCreateInfo{DescriptorType::DESCRIPTOR_TYPE_CONSTANT_BUFFER,
                                             ResourceState::RESOURCE_STATE_SHADER_RESOURCE, sizeof(GTAOConstant)});

    // Initialize GTAO constants
    m_gtao_constants.width = m_width;
    m_gtao_constants.height = m_height;
    m_gtao_constants.radius = 1.5f;
    m_gtao_constants.falloff = 2.0f;
    m_gtao_constants.thickness = 0.35f;
    m_gtao_constants.bias = 0.03f;
    m_gtao_constants.direction_count = 6;
    m_gtao_constants.step_count = 6;
    m_gtao_constants.max_pixel_radius = 48.0f;
    m_gtao_constants.intensity = 1.0f;
    SetResizeCallback([this](u32 width, u32 height) {
        m_width = width;
        m_height = height;
        m_gtao_constants.width = width;
        m_gtao_constants.height = height;
    });
}

GTAORDGPass::~GTAORDGPass()
{
    DestroyShader(m_gtao_cs);
    DestroyPipeline(m_gtao_pipeline);
    m_rhi->DestroyTexture(m_gtao_factor_image);
    m_rhi->DestroyBuffer(m_gtao_constants_buffer);
}

void GTAORDGPass::ImportResources(Horizon::Backend::FrameGraph *frame_graph)
{
    m_gtao_factor_handle = frame_graph->ImportTexture("gtao_factor", m_gtao_factor_image);
}

void GTAORDGPass::SetInputHandles(Horizon::Backend::TextureHandle depth, Horizon::Backend::TextureHandle gbuffer0)
{
    m_depth_handle = depth;
    m_gbuffer0_handle = gbuffer0;
}

void GTAORDGPass::UpdateConstants(const void *data, u32 size)
{
    // This will be called externally to update constants
}

void GTAORDGPass::Setup(Horizon::Backend::FrameGraphBuilder &builder)
{
    builder.ReadTexture(m_depth_handle, ResourceState::RESOURCE_STATE_SHADER_RESOURCE);
    builder.ReadTexture(m_gbuffer0_handle, ResourceState::RESOURCE_STATE_SHADER_RESOURCE);
    builder.WriteTexture(m_gtao_factor_handle, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);
}

void GTAORDGPass::Execute(CommandList *cl, Horizon::Backend::FrameGraphBuilder &builder)
{
    cl->BeginComputePass("GTAO Pass");
    m_gtao_pipeline->SetResource(builder.GetTexture(m_depth_handle), "depth_tex");
    m_gtao_pipeline->SetResource(builder.GetTexture(m_gbuffer0_handle), "normal_tex");
    m_gtao_pipeline->SetResource(m_sampler, "default_sampler");
    m_gtao_pipeline->SetResource(builder.GetTexture(m_gtao_factor_handle), "ao_factor_tex");
    m_gtao_pipeline->SetResource(m_gtao_constants_buffer, "GTAOConstant_cb");
    cl->BindPipeline(m_gtao_pipeline);
    cl->Dispatch(AlignUp<u32>(m_width, 8), AlignUp<u32>(m_height, 8), 1);
    cl->EndComputePass();
}
