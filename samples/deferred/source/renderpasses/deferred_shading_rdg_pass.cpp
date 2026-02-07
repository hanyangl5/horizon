#include "deferred_shading_rdg_pass.h"
#include <scene/scene_manager/scene_manager.h>

DeferredShadingRDGPass::DeferredShadingRDGPass(RHI *rhi, Horizon::SceneManager *scene_manager)
    : RDGPass("Deferred Shading Pass", rhi), m_rhi(rhi), m_scene_manager(scene_manager)
{
    // Create shader and pipeline using base class helper functions
    m_shading_cs = CreateShader(ShaderType::COMPUTE_SHADER, shader_dir / "deferred_shading.comp.hlsl", "main");
    m_shading_pipeline = CreateComputePipeline(ComputePipelineCreateInfo{});
    m_shading_pipeline->SetComputeShader(m_shading_cs);

    // Create constant buffers
    m_deferred_shading_constants_buffer = rhi->CreateBuffer(
        BufferCreateInfo{DescriptorType::DESCRIPTOR_TYPE_CONSTANT_BUFFER,
                         ResourceState::RESOURCE_STATE_SHADER_RESOURCE, sizeof(DeferredShadingConstants)});

    m_diffuse_irradiance_sh3_buffer = rhi->CreateBuffer(
        BufferCreateInfo{DescriptorType::DESCRIPTOR_TYPE_CONSTANT_BUFFER,
                         ResourceState::RESOURCE_STATE_SHADER_RESOURCE, sizeof(DiffuseIrradianceSH3)});

    // Initialize constants
    m_deferred_shading_constants.width = width;
    m_deferred_shading_constants.height = height;
    m_deferred_shading_constants.ibl_intensity = 10000.0;

    // Initialize IBL SH3 constants
    m_diffuse_irradiance_sh3_constants.sh = {
        Math::float4{0.473198890686035f, 0.519405245780945f, 0.554664373397827f, 0.0f},
        Math::float4{0.416269570589066f, 0.466901600360870f, 0.595043838024139f, 0.0f},
        Math::float4{0.070390045642853f, 0.072113677859306f, 0.075183071196079f, 0.0f},
        Math::float4{0.200731590390205f, -0.189936503767967f, -0.178353592753410f, 0.0f},
        Math::float4{0.165346711874008f, -0.156177446246147f, -0.144699439406395f, 0.0f},
        Math::float4{0.037444319576025f, 0.041276078671217f, 0.046160303056240f, 0.0f},
        Math::float4{0.007342631462961f, -0.009751657955348f, -0.015737744048238f, 0.0f},
        Math::float4{0.023010414093733f, -0.011694960296154f, 0.001283747726120f, 0.0f},
        Math::float4{0.000401695695473f, -0.013503036461771f, -0.041937090456486f, 0.0f}};

    // Load IBL textures
    m_prefilered_irradiance_env_map_data =
        TextureLoader::Load((asset_path / "envrionment/football/footballSpecularHDR.dds").c_str());
    {
        TextureCreateInfo texture_create_info{};
        texture_create_info.width = m_prefilered_irradiance_env_map_data.width;
        texture_create_info.height = m_prefilered_irradiance_env_map_data.height;
        texture_create_info.array_layer = m_prefilered_irradiance_env_map_data.layer_count;
        texture_create_info.enanble_mipmap = true;
        texture_create_info.texture_type = TextureType::TEXTURE_TYPE_CUBE;
        texture_create_info.descriptor_types = DescriptorType::DESCRIPTOR_TYPE_TEXTURE_CUBE;
        texture_create_info.initial_state = ResourceState::RESOURCE_STATE_SHADER_RESOURCE;
        texture_create_info.texture_format = m_prefilered_irradiance_env_map_data.format;
        texture_create_info.debug_name = "specular_map";
        m_prefiltered_irradiance_env_map = rhi->CreateTexture(texture_create_info);
    }

    m_brdf_lut_data_desc = TextureLoader::Load((asset_path / "envrionment/football/footballBrdf.dds").c_str());
    {
        TextureCreateInfo texture_create_info{};
        texture_create_info.width = m_brdf_lut_data_desc.width;
        texture_create_info.height = m_brdf_lut_data_desc.height;
        texture_create_info.array_layer = m_brdf_lut_data_desc.layer_count;
        texture_create_info.enanble_mipmap = false;
        texture_create_info.texture_type = TextureType::TEXTURE_TYPE_2D;
        texture_create_info.descriptor_types = DescriptorType::DESCRIPTOR_TYPE_TEXTURE_CUBE;
        texture_create_info.initial_state = ResourceState::RESOURCE_STATE_SHADER_RESOURCE;
        texture_create_info.texture_format = m_brdf_lut_data_desc.format;
        texture_create_info.debug_name = "brdf_lut";
        m_brdf_lut = rhi->CreateTexture(texture_create_info);
    }

    // Create IBL sampler
    SamplerDesc sampler_desc{};
    sampler_desc.min_filter = FilterType::FILTER_LINEAR;
    sampler_desc.mag_filter = FilterType::FILTER_LINEAR;
    sampler_desc.mip_map_mode = MipMapMode::MIPMAP_MODE_LINEAR;
    sampler_desc.address_u = AddressMode::ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_desc.address_v = AddressMode::ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_desc.address_w = AddressMode::ADDRESS_MODE_CLAMP_TO_EDGE;
    m_ibl_sampler = rhi->CreateSampler(sampler_desc);
    
    // Create shading color texture
    m_shading_color_texture = rhi->CreateTexture(TextureCreateInfo{
        DescriptorType::DESCRIPTOR_TYPE_RW_TEXTURE | DescriptorType::DESCRIPTOR_TYPE_TEXTURE, 
        ResourceState::RESOURCE_STATE_UNORDERED_ACCESS,
        TextureType::TEXTURE_TYPE_2D, TextureFormat::TEXTURE_FORMAT_R11G11B10_UFLOAT, width, height, 1, false});
}

DeferredShadingRDGPass::~DeferredShadingRDGPass()
{
    DestroyShader(m_shading_cs);
    DestroyPipeline(m_shading_pipeline);
    m_rhi->DestroyBuffer(m_deferred_shading_constants_buffer);
    m_rhi->DestroyBuffer(m_diffuse_irradiance_sh3_buffer);
    m_rhi->DestroyTexture(m_prefiltered_irradiance_env_map);
    m_rhi->DestroyTexture(m_brdf_lut);
    m_rhi->DestroyTexture(m_shading_color_texture);
    m_rhi->DestroySampler(m_ibl_sampler);
}
void DeferredShadingRDGPass::ImportResources(Horizon::Backend::FrameGraph *frame_graph)
{
    // Import IBL textures that we own
    m_brdf_lut_handle = frame_graph->ImportTexture("brdf_lut", m_brdf_lut);
    m_prefiltered_env_handle = frame_graph->ImportTexture("prefiltered_env", m_prefiltered_irradiance_env_map);
    
    // Import shading color texture
    m_shading_color_handle = frame_graph->ImportTexture("shading_color", m_shading_color_texture);
}

void DeferredShadingRDGPass::SetGBufferHandles(Horizon::Backend::TextureHandle gbuffer0,
                                               Horizon::Backend::TextureHandle gbuffer1,
                                               Horizon::Backend::TextureHandle gbuffer2,
                                               Horizon::Backend::TextureHandle gbuffer3,
                                               Horizon::Backend::TextureHandle depth)
{
    m_gbuffer0_handle = gbuffer0;
    m_gbuffer1_handle = gbuffer1;
    m_gbuffer2_handle = gbuffer2;
    m_gbuffer3_handle = gbuffer3;
    m_depth_handle = depth;
}

void DeferredShadingRDGPass::SetSSAOBlurHandle(Horizon::Backend::TextureHandle ssao_blur)
{
    m_ssao_blur_handle = ssao_blur;
}

void DeferredShadingRDGPass::Setup(Horizon::Backend::FrameGraphBuilder &builder)
{
    // Get handles from external resources (these should be set by the render app)
    // For now, we'll try to find them by name
    // In a real implementation, these would be passed in or stored as member variables

    builder.ReadTexture(m_gbuffer0_handle, ResourceState::RESOURCE_STATE_SHADER_RESOURCE);
    builder.ReadTexture(m_gbuffer1_handle, ResourceState::RESOURCE_STATE_SHADER_RESOURCE);
    builder.ReadTexture(m_gbuffer2_handle, ResourceState::RESOURCE_STATE_SHADER_RESOURCE);
    builder.ReadTexture(m_gbuffer3_handle, ResourceState::RESOURCE_STATE_SHADER_RESOURCE);
    builder.ReadTexture(m_depth_handle, ResourceState::RESOURCE_STATE_SHADER_RESOURCE);
    builder.ReadTexture(m_ssao_blur_handle, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);
    builder.ReadTexture(m_brdf_lut_handle, ResourceState::RESOURCE_STATE_SHADER_RESOURCE);
    builder.ReadTexture(m_prefiltered_env_handle, ResourceState::RESOURCE_STATE_SHADER_RESOURCE);
    builder.WriteTexture(m_shading_color_handle, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);
}

void DeferredShadingRDGPass::Execute(CommandList *cl, Horizon::Backend::FrameGraphBuilder &builder)
{
    cl->BeginComputePass("Deferred Shading Pass");
    m_shading_pipeline->SetResource(builder.GetTexture(m_gbuffer0_handle), "gbuffer0_tex");
    m_shading_pipeline->SetResource(builder.GetTexture(m_gbuffer1_handle), "gbuffer1_tex");
    m_shading_pipeline->SetResource(builder.GetTexture(m_gbuffer2_handle), "gbuffer2_tex");
    m_shading_pipeline->SetResource(builder.GetTexture(m_gbuffer3_handle), "gbuffer3_tex");
    m_shading_pipeline->SetResource(builder.GetTexture(m_depth_handle), "depth_tex");
    m_shading_pipeline->SetResource(m_deferred_shading_constants_buffer, "DeferredShadingConstants_cb");
    m_shading_pipeline->SetResource(m_scene_manager->GetLightCountBuffer(), "LightCountUb_cb");
    m_shading_pipeline->SetResource(m_scene_manager->GetLightParamBuffer(), "LightDataUb_cb");
    m_shading_pipeline->SetResource(builder.GetTexture(m_shading_color_handle), "out_color");
    m_shading_pipeline->SetResource(builder.GetTexture(m_ssao_blur_handle), "ao_tex");
    m_shading_pipeline->SetResource(m_diffuse_irradiance_sh3_buffer, "DiffuseIrradianceSH3_cb");
    m_shading_pipeline->SetResource(builder.GetTexture(m_prefiltered_env_handle), "specular_map");
    m_shading_pipeline->SetResource(builder.GetTexture(m_brdf_lut_handle), "specular_brdf_lut");
    m_shading_pipeline->SetResource(m_ibl_sampler, "ibl_sampler");
    cl->BindPipeline(m_shading_pipeline);
    cl->Dispatch(AlignUp<u32>(width, 8), AlignUp<u32>(height, 8), 1);
    cl->EndComputePass();
}
