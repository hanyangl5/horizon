#include "deferred_shading.h"
#include <scene/scene_manager/scene_manager.h>

DeferredShadingPass::DeferredShadingPass(RHI *rhi) noexcept : mRhi(rhi)
{

    // geometry pass
    {
        gbuffer0 = rhi->CreateRenderTarget(RenderTargetCreateInfo{RenderTargetFormat::TEXTURE_FORMAT_RGBA8_UNORM,
                                                                  RenderTargetType::COLOR, width, height});
        gbuffer1 = rhi->CreateRenderTarget(RenderTargetCreateInfo{RenderTargetFormat::TEXTURE_FORMAT_RGBA8_UNORM,
                                                                  RenderTargetType::COLOR, width, height});
        gbuffer2 = rhi->CreateRenderTarget(RenderTargetCreateInfo{RenderTargetFormat::TEXTURE_FORMAT_R11G11B10_UFLOAT,
                                                                  RenderTargetType::COLOR, width, height});
        gbuffer3 = rhi->CreateRenderTarget(RenderTargetCreateInfo{RenderTargetFormat::TEXTURE_FORMAT_RGBA8_UNORM,
                                                                  RenderTargetType::COLOR, width, height});
        gbuffer4 = rhi->CreateRenderTarget(RenderTargetCreateInfo{RenderTargetFormat::TEXTURE_FORMAT_RG32_SFLOAT,
                                                                  RenderTargetType::COLOR, width, height});
        depth = rhi->CreateRenderTarget(RenderTargetCreateInfo{RenderTargetFormat::TEXTURE_FORMAT_D32_SFLOAT,
                                                               RenderTargetType::DEPTH_STENCIL, width, height});

        graphics_pass_ci.vertex_input_state.attribute_count = 5;

        auto &pos = graphics_pass_ci.vertex_input_state.attributes[0];
        pos.attrib_format = VertexAttribFormat::F32; // position
        pos.portion = 3;
        pos.binding = 0;
        pos.location = 0;
        pos.offset = 0;
        pos.input_rate = VertexInputRate::VERTEX_ATTRIB_RATE_VERTEX;

        auto &normal = graphics_pass_ci.vertex_input_state.attributes[1];
        normal.attrib_format = VertexAttribFormat::F32; // normal, TOOD: SN16 is a better format
        normal.portion = 3;
        normal.binding = 0;
        normal.location = 1;
        normal.input_rate = VertexInputRate::VERTEX_ATTRIB_RATE_VERTEX;
        normal.offset = offsetof(Vertex, normal);

        auto &uv0 = graphics_pass_ci.vertex_input_state.attributes[2];
        uv0.attrib_format = VertexAttribFormat::F32; // uv0 TOOD: UN16 is a better format
        uv0.portion = 2;
        uv0.binding = 0;
        uv0.location = 2;
        uv0.input_rate = VertexInputRate::VERTEX_ATTRIB_RATE_VERTEX;
        uv0.offset = offsetof(Vertex, uv0);

        auto &uv1 = graphics_pass_ci.vertex_input_state.attributes[3];
        uv1.attrib_format = VertexAttribFormat::F32; // uv1 TOOD: UN16 is a better format
        uv1.portion = 2;
        uv1.binding = 0;
        uv1.location = 3;
        uv1.input_rate = VertexInputRate::VERTEX_ATTRIB_RATE_VERTEX;
        uv1.offset = offsetof(Vertex, uv1);

        auto &tangent = graphics_pass_ci.vertex_input_state.attributes[4];
        tangent.attrib_format = VertexAttribFormat::F32;
        tangent.portion = 3;
        tangent.binding = 0;
        tangent.location = 4;
        tangent.input_rate = VertexInputRate::VERTEX_ATTRIB_RATE_VERTEX;
        tangent.offset = offsetof(Vertex, tangent);

        graphics_pass_ci.view_port_state.width = width;
        graphics_pass_ci.view_port_state.height = height;

        graphics_pass_ci.depth_stencil_state.depth_func = DepthFunc::LESS;
        graphics_pass_ci.depth_stencil_state.depthNear = 0.0f;
        graphics_pass_ci.depth_stencil_state.depthNear = 1.0f;
        graphics_pass_ci.depth_stencil_state.depth_test = true;
        graphics_pass_ci.depth_stencil_state.depth_write = true;
        graphics_pass_ci.depth_stencil_state.stencil_enabled = false;

        graphics_pass_ci.input_assembly_state.topology = PrimitiveTopology::TRIANGLE_LIST;

        graphics_pass_ci.multi_sample_state.sample_count = 1;

        graphics_pass_ci.rasterization_state.cull_mode = CullMode::BACK;
        graphics_pass_ci.rasterization_state.discard = false;
        graphics_pass_ci.rasterization_state.fill_mode = FillMode::TRIANGLE;
        graphics_pass_ci.rasterization_state.front_face = FrontFace::CCW;

        graphics_pass_ci.render_target_formats.color_attachment_count = 5;

        graphics_pass_ci.render_target_formats.color_attachment_formats = std::vector<TextureFormat>{
            gbuffer0->GetTexture()->m_format, gbuffer1->GetTexture()->m_format, gbuffer2->GetTexture()->m_format,
            gbuffer3->GetTexture()->m_format, gbuffer4->GetTexture()->m_format};
        graphics_pass_ci.render_target_formats.has_depth = true;
        graphics_pass_ci.render_target_formats.depth_stencil_format = depth->GetTexture()->m_format;

        geometry_pass = rhi->CreateGraphicsPipeline(graphics_pass_ci);
    }

    {
        geometry_vs = rhi->CreateShader(ShaderType::VERTEX_SHADER, shader_dir / "gbuffer_bindless.hlsl", "vs_main");

        geometry_ps = rhi->CreateShader(ShaderType::PIXEL_SHADER, shader_dir / "gbuffer_bindless.hlsl", "ps_main");

        shading_cs = rhi->CreateShader(ShaderType::COMPUTE_SHADER, shader_dir / "deferred_shading.comp.hlsl", "main");
    }

    {
        deferred_shading_constants_buffer = rhi->CreateBuffer(
            BufferCreateInfo{DescriptorType::DESCRIPTOR_TYPE_CONSTANT_BUFFER,
                             ResourceState::RESOURCE_STATE_SHADER_RESOURCE, sizeof(DeferredShadingConstants)});

        diffuse_irradiance_sh3_buffer = rhi->CreateBuffer(
            BufferCreateInfo{DescriptorType::DESCRIPTOR_TYPE_CONSTANT_BUFFER,
                             ResourceState::RESOURCE_STATE_SHADER_RESOURCE, sizeof(DiffuseIrradianceSH3)});
    }

    // SHADING PASS
    shading_pass = rhi->CreateComputePipeline(ComputePipelineCreateInfo{});
    shading_color_image = rhi->CreateTexture(TextureCreateInfo{
        DescriptorType::DESCRIPTOR_TYPE_RW_TEXTURE, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS,
        TextureType::TEXTURE_TYPE_2D, TextureFormat::TEXTURE_FORMAT_R11G11B10_UFLOAT, width, height, 1, false});
    {

        // ibl

        diffuse_irradiance_sh3_constants.sh = {
            Math::float4{0.473198890686035f, 0.519405245780945f, 0.554664373397827f, 0.0f},
            Math::float4{0.416269570589066f, 0.466901600360870f, 0.595043838024139f, 0.0f},
            Math::float4{0.070390045642853f, 0.072113677859306f, 0.075183071196079f, 0.0f},
            Math::float4{0.200731590390205f, -0.189936503767967f, -0.178353592753410f, 0.0f},
            Math::float4{0.165346711874008f, -0.156177446246147f, -0.144699439406395f, 0.0f},
            Math::float4{0.037444319576025f, 0.041276078671217f, 0.046160303056240f, 0.0f},
            Math::float4{0.007342631462961f, -0.009751657955348f, -0.015737744048238f, 0.0f},
            Math::float4{0.023010414093733f, -0.011694960296154f, 0.001283747726120f, 0.0f},
            Math::float4{0.000401695695473f, -0.013503036461771f, -0.041937090456486f, 0.0f}};

        prefilered_irradiance_env_map_data =
            TextureLoader::Load((asset_path / "envrionment/football/footballSpecularHDR.dds").c_str());

        {
            TextureCreateInfo texture_create_info{};
            texture_create_info.width = prefilered_irradiance_env_map_data.width;
            texture_create_info.height = prefilered_irradiance_env_map_data.height;
            texture_create_info.array_layer = prefilered_irradiance_env_map_data.layer_count;
            texture_create_info.enanble_mipmap = true;
            texture_create_info.texture_type = TextureType::TEXTURE_TYPE_CUBE;
            texture_create_info.descriptor_types = DescriptorType::DESCRIPTOR_TYPE_TEXTURE_CUBE;
            texture_create_info.initial_state = ResourceState::RESOURCE_STATE_SHADER_RESOURCE;
            texture_create_info.texture_format = prefilered_irradiance_env_map_data.format;
            texture_create_info.debug_name = "specular_map";
            prefiltered_irradiance_env_map = rhi->CreateTexture(texture_create_info);
        }
        brdf_lut_data_desc = TextureLoader::Load((asset_path / "envrionment/football/footballBrdf.dds").c_str());
        {
            TextureCreateInfo texture_create_info{};
            texture_create_info.width = brdf_lut_data_desc.width;
            texture_create_info.height = brdf_lut_data_desc.height;
            texture_create_info.array_layer = brdf_lut_data_desc.layer_count;
            texture_create_info.enanble_mipmap = false;
            texture_create_info.texture_type = TextureType::TEXTURE_TYPE_2D;
            texture_create_info.descriptor_types = DescriptorType::DESCRIPTOR_TYPE_TEXTURE_CUBE;
            texture_create_info.initial_state = ResourceState::RESOURCE_STATE_SHADER_RESOURCE;
            texture_create_info.texture_format = brdf_lut_data_desc.format;
            texture_create_info.debug_name = "brdf_lut";
            brdf_lut = rhi->CreateTexture(texture_create_info);
        }
    }

    SamplerDesc sampler_desc{};
    sampler_desc.min_filter = FilterType::FILTER_LINEAR;
    sampler_desc.mag_filter = FilterType::FILTER_LINEAR;
    sampler_desc.mip_map_mode = MipMapMode::MIPMAP_MODE_LINEAR;
    sampler_desc.address_u = AddressMode::ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_desc.address_v = AddressMode::ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_desc.address_w = AddressMode::ADDRESS_MODE_CLAMP_TO_EDGE;

    ibl_sampler = rhi->CreateSampler(sampler_desc);

    deferred_shading_constants.width = width;
    deferred_shading_constants.height = height;
    deferred_shading_constants.ibl_intensity = 10000.0;

    geometry_pass->SetGraphicsShader(geometry_vs, geometry_ps);
    shading_pass->SetComputeShader(shading_cs);
}
DeferredShadingPass::~DeferredShadingPass() noexcept
{
    mRhi->DestroyShader(geometry_vs);
    mRhi->DestroyShader(geometry_ps);

    mRhi->DestroyShader(shading_cs);

    mRhi->DestroyPipeline(geometry_pass);
    mRhi->DestroyPipeline(shading_pass);

    mRhi->DestroyBuffer(deferred_shading_constants_buffer);

    mRhi->DestroyRenderTarget(depth);
    mRhi->DestroyRenderTarget(gbuffer0);
    mRhi->DestroyRenderTarget(gbuffer1);
    mRhi->DestroyRenderTarget(gbuffer2);
    mRhi->DestroyRenderTarget(gbuffer3);
    mRhi->DestroyRenderTarget(gbuffer4);

    mRhi->DestroyTexture(shading_color_image);

    mRhi->DestroyBuffer(diffuse_irradiance_sh3_buffer);
    mRhi->DestroyTexture(brdf_lut);
    mRhi->DestroyTexture(prefiltered_irradiance_env_map);
    mRhi->DestroySampler(ibl_sampler);
}

void DeferredShadingPass::ImportResources(Horizon::Backend::FrameGraph *frame_graph,
                                           Horizon::Backend::TextureHandle &gbuffer0_handle,
                                           Horizon::Backend::TextureHandle &gbuffer1_handle,
                                           Horizon::Backend::TextureHandle &gbuffer2_handle,
                                           Horizon::Backend::TextureHandle &gbuffer3_handle,
                                           Horizon::Backend::TextureHandle &gbuffer4_handle,
                                           Horizon::Backend::TextureHandle &depth_handle,
                                           Horizon::Backend::TextureHandle &shading_color_handle,
                                           Horizon::Backend::RenderTargetHandle &gbuffer0_rt_handle,
                                           Horizon::Backend::RenderTargetHandle &gbuffer1_rt_handle,
                                           Horizon::Backend::RenderTargetHandle &gbuffer2_rt_handle,
                                           Horizon::Backend::RenderTargetHandle &gbuffer3_rt_handle,
                                           Horizon::Backend::RenderTargetHandle &gbuffer4_rt_handle,
                                           Horizon::Backend::RenderTargetHandle &depth_rt_handle,
                                           Horizon::Backend::TextureHandle &brdf_lut_handle,
                                           Horizon::Backend::TextureHandle &prefiltered_env_handle)
{
    gbuffer0_rt_handle = frame_graph->ImportRenderTarget("gbuffer0_rt", gbuffer0);
    gbuffer1_rt_handle = frame_graph->ImportRenderTarget("gbuffer1_rt", gbuffer1);
    gbuffer2_rt_handle = frame_graph->ImportRenderTarget("gbuffer2_rt", gbuffer2);
    gbuffer3_rt_handle = frame_graph->ImportRenderTarget("gbuffer3_rt", gbuffer3);
    gbuffer4_rt_handle = frame_graph->ImportRenderTarget("gbuffer4_rt", gbuffer4);
    depth_rt_handle = frame_graph->ImportRenderTarget("depth_rt", depth);

    gbuffer0_handle = frame_graph->ImportTexture("gbuffer0", gbuffer0->GetTexture());
    gbuffer1_handle = frame_graph->ImportTexture("gbuffer1", gbuffer1->GetTexture());
    gbuffer2_handle = frame_graph->ImportTexture("gbuffer2", gbuffer2->GetTexture());
    gbuffer3_handle = frame_graph->ImportTexture("gbuffer3", gbuffer3->GetTexture());
    gbuffer4_handle = frame_graph->ImportTexture("gbuffer4", gbuffer4->GetTexture());
    depth_handle = frame_graph->ImportTexture("depth", depth->GetTexture());

    shading_color_handle = frame_graph->ImportTexture("shading_color", shading_color_image);
    brdf_lut_handle = frame_graph->ImportTexture("brdf_lut", brdf_lut);
    prefiltered_env_handle = frame_graph->ImportTexture("prefiltered_env", prefiltered_irradiance_env_map);
}

void DeferredShadingPass::SetupGeometryPass(Horizon::Backend::FrameGraphBuilder &builder,
                                            Horizon::Backend::RenderTargetHandle gbuffer0_rt_handle,
                                            Horizon::Backend::RenderTargetHandle gbuffer1_rt_handle,
                                            Horizon::Backend::RenderTargetHandle gbuffer2_rt_handle,
                                            Horizon::Backend::RenderTargetHandle gbuffer3_rt_handle,
                                            Horizon::Backend::RenderTargetHandle gbuffer4_rt_handle,
                                            Horizon::Backend::RenderTargetHandle depth_rt_handle,
                                            Horizon::Backend::TextureHandle gbuffer0_handle,
                                            Horizon::Backend::TextureHandle gbuffer1_handle,
                                            Horizon::Backend::TextureHandle gbuffer2_handle,
                                            Horizon::Backend::TextureHandle gbuffer3_handle,
                                            Horizon::Backend::TextureHandle gbuffer4_handle,
                                            Horizon::Backend::TextureHandle depth_handle)
{
    builder.UseRenderTarget(gbuffer0_rt_handle);
    builder.UseRenderTarget(gbuffer1_rt_handle);
    builder.UseRenderTarget(gbuffer2_rt_handle);
    builder.UseRenderTarget(gbuffer3_rt_handle);
    builder.UseRenderTarget(gbuffer4_rt_handle);
    builder.UseRenderTarget(depth_rt_handle);

    builder.WriteTexture(gbuffer0_handle, ResourceState::RESOURCE_STATE_RENDER_TARGET);
    builder.WriteTexture(gbuffer1_handle, ResourceState::RESOURCE_STATE_RENDER_TARGET);
    builder.WriteTexture(gbuffer2_handle, ResourceState::RESOURCE_STATE_RENDER_TARGET);
    builder.WriteTexture(gbuffer3_handle, ResourceState::RESOURCE_STATE_RENDER_TARGET);
    builder.WriteTexture(gbuffer4_handle, ResourceState::RESOURCE_STATE_RENDER_TARGET);
    builder.WriteTexture(depth_handle, ResourceState::RESOURCE_STATE_DEPTH_WRITE);
}

void DeferredShadingPass::ExecuteGeometryPass(CommandList *cl, Horizon::Backend::FrameGraphBuilder &builder,
                                              Horizon::Backend::RenderTargetHandle gbuffer0_rt_handle,
                                              Horizon::Backend::RenderTargetHandle gbuffer1_rt_handle,
                                              Horizon::Backend::RenderTargetHandle gbuffer2_rt_handle,
                                              Horizon::Backend::RenderTargetHandle gbuffer3_rt_handle,
                                              Horizon::Backend::RenderTargetHandle gbuffer4_rt_handle,
                                              Horizon::Backend::RenderTargetHandle depth_rt_handle,
    Horizon::SceneManager *scene_manager, Sampler *sampler,
                                              Buffer *taa_prev_curr_offset_buffer)
{
    RenderPassBeginInfo begin_info{};
    begin_info.render_target_count = 5;
    begin_info.render_area = Rect{0, 0, width, height};
    begin_info.render_targets[0].data = builder.GetRenderTarget(gbuffer0_rt_handle);
    begin_info.render_targets[0].clear_color = {};
    begin_info.render_targets[0].load_op = RenderTargetLoadOp::CLEAR;
    begin_info.render_targets[0].store_op = RenderTargetStoreOp::STORE;
    begin_info.render_targets[1].data = builder.GetRenderTarget(gbuffer1_rt_handle);
    begin_info.render_targets[1].clear_color = {};
    begin_info.render_targets[1].load_op = RenderTargetLoadOp::CLEAR;
    begin_info.render_targets[1].store_op = RenderTargetStoreOp::STORE;
    begin_info.render_targets[2].data = builder.GetRenderTarget(gbuffer2_rt_handle);
    begin_info.render_targets[2].clear_color = {};
    begin_info.render_targets[2].load_op = RenderTargetLoadOp::CLEAR;
    begin_info.render_targets[2].store_op = RenderTargetStoreOp::STORE;
    begin_info.render_targets[3].data = builder.GetRenderTarget(gbuffer3_rt_handle);
    begin_info.render_targets[3].clear_color = {};
    begin_info.render_targets[3].load_op = RenderTargetLoadOp::CLEAR;
    begin_info.render_targets[3].store_op = RenderTargetStoreOp::STORE;
    begin_info.render_targets[4].data = builder.GetRenderTarget(gbuffer4_rt_handle);
    begin_info.render_targets[4].clear_color = {};
    begin_info.render_targets[4].load_op = RenderTargetLoadOp::CLEAR;
    begin_info.render_targets[4].store_op = RenderTargetStoreOp::STORE;
    begin_info.depth_stencil.data = builder.GetRenderTarget(depth_rt_handle);
    begin_info.depth_stencil.clear_color = ClearValueDepthStencil{1.0, 0};
    begin_info.depth_stencil.load_op = RenderTargetLoadOp::CLEAR;
    begin_info.depth_stencil.store_op = RenderTargetStoreOp::STORE;
    begin_info.debug_name = "Geometry Pass";

    cl->BeginRenderPass(begin_info);

    // Setup resources
    geometry_pass->SetResource(scene_manager->GetCameraBuffer(), "CameraParamsUb_cb");
    geometry_pass->SetResource(scene_manager->instance_parameter_buffer, "instance_parameter");
    geometry_pass->SetResource(scene_manager->material_description_buffer, "material_descriptions");
    geometry_pass->SetResource(sampler, "default_sampler");
    geometry_pass->SetResource(taa_prev_curr_offset_buffer, "TAAOffsets_cb");

    std::vector<Texture *> material_textures;
    for (auto &tex : scene_manager->material_textures)
    {
        material_textures.push_back(tex);
    }
    geometry_pass->SetBindlessResource(material_textures, "material_textures");
    cl->BindPipeline(geometry_pass);

    for (u32 mesh_data = 0; mesh_data < scene_manager->mesh_data.size(); mesh_data++)
    {
        auto &mesh = scene_manager->mesh_data[mesh_data];
        auto ib = scene_manager->index_buffers[mesh.index_buffer_offset];
        auto vb = scene_manager->vertex_buffers[mesh.vertex_buffer_offset];
        u32 offset = 0;
        cl->BindVertexBuffers(1, &vb, &offset);
        cl->BindIndexBuffer(ib, 0);
        cl->BindPushConstant(geometry_pass, "mesh_draw_offset", &mesh.draw_offset);

        cl->DrawIndirectIndexedInstanced(scene_manager->indirect_draw_command_buffer1,
                                         sizeof(DrawIndexedInstancedCommand) * mesh.draw_offset, mesh.draw_count,
                                         sizeof(DrawIndexedInstancedCommand));
    }

    cl->EndRenderPass();
}

void DeferredShadingPass::SetupDeferredShadingPass(Horizon::Backend::FrameGraphBuilder &builder,
                                                    Horizon::Backend::TextureHandle gbuffer0_handle,
                                                    Horizon::Backend::TextureHandle gbuffer1_handle,
                                                    Horizon::Backend::TextureHandle gbuffer2_handle,
                                                    Horizon::Backend::TextureHandle gbuffer3_handle,
                                                    Horizon::Backend::TextureHandle depth_handle,
                                                    Horizon::Backend::TextureHandle shading_color_handle,
                                                    Horizon::Backend::TextureHandle ssao_blur_handle,
                                                    Horizon::Backend::TextureHandle brdf_lut_handle,
                                                    Horizon::Backend::TextureHandle prefiltered_env_handle)
{
    builder.ReadTexture(gbuffer0_handle, ResourceState::RESOURCE_STATE_SHADER_RESOURCE);
    builder.ReadTexture(gbuffer1_handle, ResourceState::RESOURCE_STATE_SHADER_RESOURCE);
    builder.ReadTexture(gbuffer2_handle, ResourceState::RESOURCE_STATE_SHADER_RESOURCE);
    builder.ReadTexture(gbuffer3_handle, ResourceState::RESOURCE_STATE_SHADER_RESOURCE);
    builder.ReadTexture(depth_handle, ResourceState::RESOURCE_STATE_SHADER_RESOURCE);
    builder.ReadTexture(ssao_blur_handle, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);
    builder.ReadTexture(brdf_lut_handle, ResourceState::RESOURCE_STATE_SHADER_RESOURCE);
    builder.ReadTexture(prefiltered_env_handle, ResourceState::RESOURCE_STATE_SHADER_RESOURCE);
    builder.WriteTexture(shading_color_handle, ResourceState::RESOURCE_STATE_UNORDERED_ACCESS);
}

void DeferredShadingPass::ExecuteDeferredShadingPass(CommandList *cl, Horizon::Backend::FrameGraphBuilder &builder,
                                                     Horizon::Backend::TextureHandle gbuffer0_handle,
                                                     Horizon::Backend::TextureHandle gbuffer1_handle,
                                                     Horizon::Backend::TextureHandle gbuffer2_handle,
                                                     Horizon::Backend::TextureHandle gbuffer3_handle,
                                                     Horizon::Backend::TextureHandle depth_handle,
                                                     Horizon::Backend::TextureHandle shading_color_handle,
                                                     Horizon::Backend::TextureHandle ssao_blur_handle,
                                                     Horizon::Backend::TextureHandle brdf_lut_handle,
                                                     Horizon::Backend::TextureHandle prefiltered_env_handle,
    Horizon::SceneManager *scene_manager)
{
    cl->BeginComputePass("Deferred Shading Pass");
    shading_pass->SetResource(builder.GetTexture(gbuffer0_handle), "gbuffer0_tex");
    shading_pass->SetResource(builder.GetTexture(gbuffer1_handle), "gbuffer1_tex");
    shading_pass->SetResource(builder.GetTexture(gbuffer2_handle), "gbuffer2_tex");
    shading_pass->SetResource(builder.GetTexture(gbuffer3_handle), "gbuffer3_tex");
    shading_pass->SetResource(builder.GetTexture(depth_handle), "depth_tex");
    shading_pass->SetResource(deferred_shading_constants_buffer, "DeferredShadingConstants_cb");
    shading_pass->SetResource(scene_manager->GetLightCountBuffer(), "LightCountUb_cb");
    shading_pass->SetResource(scene_manager->GetLightParamBuffer(), "LightDataUb_cb");
    shading_pass->SetResource(builder.GetTexture(shading_color_handle), "out_color");
    shading_pass->SetResource(builder.GetTexture(ssao_blur_handle), "ao_tex");
    shading_pass->SetResource(diffuse_irradiance_sh3_buffer, "DiffuseIrradianceSH3_cb");
    shading_pass->SetResource(builder.GetTexture(prefiltered_env_handle), "specular_map");
    shading_pass->SetResource(builder.GetTexture(brdf_lut_handle), "specular_brdf_lut");
    shading_pass->SetResource(ibl_sampler, "ibl_sampler");
    cl->BindPipeline(shading_pass);
    cl->Dispatch(AlignUp<u32>(width, 8), AlignUp<u32>(height, 8), 1);
    cl->EndComputePass();
}