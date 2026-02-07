#include "deferred_geometry_rdg_pass.h"
#include <scene/scene_manager/scene_manager.h>
#include "taa_rdg_pass.h"

DeferredShadingGeometryPass::DeferredShadingGeometryPass(RHI *rhi, Horizon::SceneManager *scene_manager, Sampler *sampler)
    : RDGPass("Geometry Pass", rhi), m_rhi(rhi), m_scene_manager(scene_manager), m_sampler(sampler)
{
    // Create shaders and pipeline using base class helper functions
    m_geometry_vs = CreateShader(ShaderType::VERTEX_SHADER, shader_dir / "gbuffer_bindless.hlsl", "vs_main");
    m_geometry_ps = CreateShader(ShaderType::PIXEL_SHADER, shader_dir / "gbuffer_bindless.hlsl", "ps_main");

    GraphicsPipelineCreateInfo graphics_pass_ci{};
    graphics_pass_ci.vertex_input_state.attribute_count = 5;

    auto &pos = graphics_pass_ci.vertex_input_state.attributes[0];
    pos.attrib_format = VertexAttribFormat::F32;
    pos.portion = 3;
    pos.binding = 0;
    pos.location = 0;
    pos.offset = 0;
    pos.input_rate = VertexInputRate::VERTEX_ATTRIB_RATE_VERTEX;

    auto &normal = graphics_pass_ci.vertex_input_state.attributes[1];
    normal.attrib_format = VertexAttribFormat::F32;
    normal.portion = 3;
    normal.binding = 0;
    normal.location = 1;
    normal.input_rate = VertexInputRate::VERTEX_ATTRIB_RATE_VERTEX;
    normal.offset = offsetof(Vertex, normal);

    auto &uv0 = graphics_pass_ci.vertex_input_state.attributes[2];
    uv0.attrib_format = VertexAttribFormat::F32;
    uv0.portion = 2;
    uv0.binding = 0;
    uv0.location = 2;
    uv0.input_rate = VertexInputRate::VERTEX_ATTRIB_RATE_VERTEX;
    uv0.offset = offsetof(Vertex, uv0);

    auto &uv1 = graphics_pass_ci.vertex_input_state.attributes[3];
    uv1.attrib_format = VertexAttribFormat::F32;
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
    graphics_pass_ci.depth_stencil_state.depthFar = 1.0f;
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
        TextureFormat::TEXTURE_FORMAT_RGBA8_UNORM, TextureFormat::TEXTURE_FORMAT_RGBA8_UNORM,
        TextureFormat::TEXTURE_FORMAT_R11G11B10_UFLOAT, TextureFormat::TEXTURE_FORMAT_RGBA8_UNORM,
        TextureFormat::TEXTURE_FORMAT_RG32_SFLOAT};
    graphics_pass_ci.render_target_formats.has_depth = true;
    graphics_pass_ci.render_target_formats.depth_stencil_format = TextureFormat::TEXTURE_FORMAT_D32_SFLOAT;

    m_geometry_pipeline = CreateGraphicsPipeline(graphics_pass_ci);
    m_geometry_pipeline->SetGraphicsShader(m_geometry_vs, m_geometry_ps);

    // Create render targets
    m_gbuffer0_rt = rhi->CreateRenderTarget(
        RenderTargetCreateInfo{RenderTargetFormat::TEXTURE_FORMAT_RGBA8_UNORM, RenderTargetType::COLOR, width, height});
    m_gbuffer1_rt = rhi->CreateRenderTarget(
        RenderTargetCreateInfo{RenderTargetFormat::TEXTURE_FORMAT_RGBA8_UNORM, RenderTargetType::COLOR, width, height});
    m_gbuffer2_rt = rhi->CreateRenderTarget(RenderTargetCreateInfo{RenderTargetFormat::TEXTURE_FORMAT_R11G11B10_UFLOAT,
                                                                   RenderTargetType::COLOR, width, height});
    m_gbuffer3_rt = rhi->CreateRenderTarget(
        RenderTargetCreateInfo{RenderTargetFormat::TEXTURE_FORMAT_RGBA8_UNORM, RenderTargetType::COLOR, width, height});
    m_gbuffer4_rt = rhi->CreateRenderTarget(
        RenderTargetCreateInfo{RenderTargetFormat::TEXTURE_FORMAT_RG32_SFLOAT, RenderTargetType::COLOR, width, height});
    m_depth_rt = rhi->CreateRenderTarget(RenderTargetCreateInfo{RenderTargetFormat::TEXTURE_FORMAT_D32_SFLOAT,
                                                                RenderTargetType::DEPTH_STENCIL, width, height});

    // Create TAA buffer
    m_taa_prev_curr_offset_buffer = rhi->CreateBuffer(BufferCreateInfo{DescriptorType::DESCRIPTOR_TYPE_CONSTANT_BUFFER,
                                                                       ResourceState::RESOURCE_STATE_SHADER_RESOURCE,
                                                                       sizeof(TAARDGPass::TAAPrevCurrOffset)});
}

DeferredShadingGeometryPass::~DeferredShadingGeometryPass()
{
    DestroyShader(m_geometry_vs);
    DestroyShader(m_geometry_ps);
    DestroyPipeline(m_geometry_pipeline);
    m_rhi->DestroyRenderTarget(m_gbuffer0_rt);
    m_rhi->DestroyRenderTarget(m_gbuffer1_rt);
    m_rhi->DestroyRenderTarget(m_gbuffer2_rt);
    m_rhi->DestroyRenderTarget(m_gbuffer3_rt);
    m_rhi->DestroyRenderTarget(m_gbuffer4_rt);
    m_rhi->DestroyRenderTarget(m_depth_rt);
    m_rhi->DestroyBuffer(m_taa_prev_curr_offset_buffer);
}
void DeferredShadingGeometryPass::ImportResources(Horizon::Backend::FrameGraph *frame_graph)
{
    // Import render targets into FrameGraph for state tracking
    m_gbuffer0_rt_handle = frame_graph->ImportRenderTarget("gbuffer0_rt", m_gbuffer0_rt);
    m_gbuffer1_rt_handle = frame_graph->ImportRenderTarget("gbuffer1_rt", m_gbuffer1_rt);
    m_gbuffer2_rt_handle = frame_graph->ImportRenderTarget("gbuffer2_rt", m_gbuffer2_rt);
    m_gbuffer3_rt_handle = frame_graph->ImportRenderTarget("gbuffer3_rt", m_gbuffer3_rt);
    m_gbuffer4_rt_handle = frame_graph->ImportRenderTarget("gbuffer4_rt", m_gbuffer4_rt);
    m_depth_rt_handle = frame_graph->ImportRenderTarget("depth_rt", m_depth_rt);

    // Import textures for state tracking
    m_gbuffer0_handle = frame_graph->ImportTexture("gbuffer0", m_gbuffer0_rt->GetTexture());
    m_gbuffer1_handle = frame_graph->ImportTexture("gbuffer1", m_gbuffer1_rt->GetTexture());
    m_gbuffer2_handle = frame_graph->ImportTexture("gbuffer2", m_gbuffer2_rt->GetTexture());
    m_gbuffer3_handle = frame_graph->ImportTexture("gbuffer3", m_gbuffer3_rt->GetTexture());
    m_gbuffer4_handle = frame_graph->ImportTexture("gbuffer4", m_gbuffer4_rt->GetTexture());
    m_depth_handle = frame_graph->ImportTexture("depth", m_depth_rt->GetTexture());
}


void DeferredShadingGeometryPass::Setup(Horizon::Backend::FrameGraphBuilder &builder)
{
    builder.UseRenderTarget(m_gbuffer0_rt_handle);
    builder.UseRenderTarget(m_gbuffer1_rt_handle);
    builder.UseRenderTarget(m_gbuffer2_rt_handle);
    builder.UseRenderTarget(m_gbuffer3_rt_handle);
    builder.UseRenderTarget(m_gbuffer4_rt_handle);
    builder.UseRenderTarget(m_depth_rt_handle);

    builder.WriteTexture(m_gbuffer0_handle, ResourceState::RESOURCE_STATE_RENDER_TARGET);
    builder.WriteTexture(m_gbuffer1_handle, ResourceState::RESOURCE_STATE_RENDER_TARGET);
    builder.WriteTexture(m_gbuffer2_handle, ResourceState::RESOURCE_STATE_RENDER_TARGET);
    builder.WriteTexture(m_gbuffer3_handle, ResourceState::RESOURCE_STATE_RENDER_TARGET);
    builder.WriteTexture(m_gbuffer4_handle, ResourceState::RESOURCE_STATE_RENDER_TARGET);
    builder.WriteTexture(m_depth_handle, ResourceState::RESOURCE_STATE_DEPTH_WRITE);
}

void DeferredShadingGeometryPass::Execute(CommandList *cl, Horizon::Backend::FrameGraphBuilder &builder)
{
    RenderPassBeginInfo begin_info{};
    begin_info.render_target_count = 5;
    begin_info.render_area = Rect{0, 0, width, height};
    begin_info.render_targets[0].data = builder.GetRenderTarget(m_gbuffer0_rt_handle);
    begin_info.render_targets[0].clear_color = {};
    begin_info.render_targets[0].load_op = RenderTargetLoadOp::CLEAR;
    begin_info.render_targets[0].store_op = RenderTargetStoreOp::STORE;
    begin_info.render_targets[1].data = builder.GetRenderTarget(m_gbuffer1_rt_handle);
    begin_info.render_targets[1].clear_color = {};
    begin_info.render_targets[1].load_op = RenderTargetLoadOp::CLEAR;
    begin_info.render_targets[1].store_op = RenderTargetStoreOp::STORE;
    begin_info.render_targets[2].data = builder.GetRenderTarget(m_gbuffer2_rt_handle);
    begin_info.render_targets[2].clear_color = {};
    begin_info.render_targets[2].load_op = RenderTargetLoadOp::CLEAR;
    begin_info.render_targets[2].store_op = RenderTargetStoreOp::STORE;
    begin_info.render_targets[3].data = builder.GetRenderTarget(m_gbuffer3_rt_handle);
    begin_info.render_targets[3].clear_color = {};
    begin_info.render_targets[3].load_op = RenderTargetLoadOp::CLEAR;
    begin_info.render_targets[3].store_op = RenderTargetStoreOp::STORE;
    begin_info.render_targets[4].data = builder.GetRenderTarget(m_gbuffer4_rt_handle);
    begin_info.render_targets[4].clear_color = {};
    begin_info.render_targets[4].load_op = RenderTargetLoadOp::CLEAR;
    begin_info.render_targets[4].store_op = RenderTargetStoreOp::STORE;
    begin_info.depth_stencil.data = builder.GetRenderTarget(m_depth_rt_handle);
    begin_info.depth_stencil.clear_color = ClearValueDepthStencil{1.0, 0};
    begin_info.depth_stencil.load_op = RenderTargetLoadOp::CLEAR;
    begin_info.depth_stencil.store_op = RenderTargetStoreOp::STORE;
    begin_info.debug_name = "Geometry Pass";


    // Setup resources
    m_geometry_pipeline->SetResource(m_scene_manager->GetCameraBuffer(), "CameraParamsUb_cb");
    m_geometry_pipeline->SetResource(m_scene_manager->instance_parameter_buffer, "instance_parameter");
    m_geometry_pipeline->SetResource(m_scene_manager->material_description_buffer, "material_descriptions");
    m_geometry_pipeline->SetResource(m_sampler, "default_sampler");
    m_geometry_pipeline->SetResource(m_taa_prev_curr_offset_buffer, "TAAOffsets_cb");

    std::vector<Texture *> material_textures;
    for (auto &tex : m_scene_manager->material_textures)
    {
        material_textures.push_back(tex);
    }
    m_geometry_pipeline->SetBindlessResource(material_textures, "material_textures");
    cl->BeginRenderPass(begin_info);

    cl->BindPipeline(m_geometry_pipeline);
    for (u32 mesh_data = 0; mesh_data < m_scene_manager->mesh_data.size(); mesh_data++)
    {
        auto &mesh = m_scene_manager->mesh_data[mesh_data];
        auto ib = m_scene_manager->index_buffers[mesh.index_buffer_offset];
        auto vb = m_scene_manager->vertex_buffers[mesh.vertex_buffer_offset];
        u32 offset = 0;
        cl->BindVertexBuffers(1, &vb, &offset);
        cl->BindIndexBuffer(ib, 0);
        cl->BindPushConstant(m_geometry_pipeline, "mesh_draw_offset", &mesh.draw_offset);

        cl->DrawIndirectIndexedInstanced(m_scene_manager->indirect_draw_command_buffer1,
                                         sizeof(DrawIndexedInstancedCommand) * mesh.draw_offset, mesh.draw_count,
                                         sizeof(DrawIndexedInstancedCommand));
    }

    cl->EndRenderPass();
}
