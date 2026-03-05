#include "deferred_geometry_rdg_pass.h"
#include "taa_rdg_pass.h"
#include <algorithm>
#include <scene/scene_manager/scene_manager.h>
DeferredShadingGeometryPass::DeferredShadingGeometryPass(RHI *rhi, Horizon::SceneManager *scene_manager,
                                                         Sampler *sampler, u32 width, u32 height)
    : RDGPass("Geometry Pass", rhi), m_rhi(rhi), m_scene_manager(scene_manager), m_sampler(sampler), m_width(width),
      m_height(height)
{
    const bool use_mesh_shader_from_config = GenericPlatformConfig::use_mesh_shader();
    const bool mesh_shader_supported = (m_rhi != nullptr) && m_rhi->SupportsMeshShader();
    m_use_mesh_shader_path = use_mesh_shader_from_config && mesh_shader_supported;

    if (!use_mesh_shader_from_config)
    {
        LOG_INFO("Mesh shader path is disabled by config.toml.");
    }
    else if (!mesh_shader_supported)
    {
        LOG_INFO("Mesh shader path is unsupported on current backend/GPU, using raster path.");
    }

    if (m_use_mesh_shader_path)
    {
        m_geometry_task_shader =
            CreateShader(ShaderType::TASK_SHADER, shader_dir / "gbuffer_meshshader.hlsl", "ts_main");
        m_geometry_mesh_shader =
            CreateShader(ShaderType::MESH_SHADER, shader_dir / "gbuffer_meshshader.hlsl", "ms_main");
        m_geometry_ps = CreateShader(ShaderType::PIXEL_SHADER, shader_dir / "gbuffer_meshshader.hlsl", "ps_main");
    }
    else
    {
        m_geometry_static_vs = CreateShader(ShaderType::VERTEX_SHADER, shader_dir / "gbuffer_bindless.hlsl", "vs_main");
        m_geometry_skinned_vs =
            CreateShader(ShaderType::VERTEX_SHADER, shader_dir / "gbuffer_skinned_vs.hlsl", "vs_main_skinned");
        m_geometry_ps = CreateShader(ShaderType::PIXEL_SHADER, shader_dir / "gbuffer_bindless.hlsl", "ps_main");
    }

    auto setup_common_pipeline_state = [this](GraphicsPipelineCreateInfo &ci) {
        ci.view_port_state.width = m_width;
        ci.view_port_state.height = m_height;

        ci.depth_stencil_state.depth_func = DepthFunc::LESS;
        ci.depth_stencil_state.depthNear = 0.0f;
        ci.depth_stencil_state.depthFar = 1.0f;
        ci.depth_stencil_state.depth_test = true;
        ci.depth_stencil_state.depth_write = true;
        ci.depth_stencil_state.stencil_enabled = false;

        ci.input_assembly_state.topology = PrimitiveTopology::TRIANGLE_LIST;
        ci.multi_sample_state.sample_count = 1;

        ci.rasterization_state.cull_mode = CullMode::NONE;
        ci.rasterization_state.discard = false;
        ci.rasterization_state.fill_mode = FillMode::TRIANGLE;
        ci.rasterization_state.front_face = FrontFace::CCW;

        ci.render_target_formats.color_attachment_count = 5;
        ci.render_target_formats.color_attachment_formats = std::vector<TextureFormat>{
            TextureFormat::TEXTURE_FORMAT_RGBA8_UNORM, TextureFormat::TEXTURE_FORMAT_RGBA8_UNORM,
            TextureFormat::TEXTURE_FORMAT_R11G11B10_UFLOAT, TextureFormat::TEXTURE_FORMAT_RGBA8_UNORM,
            TextureFormat::TEXTURE_FORMAT_RG32_SFLOAT};
        ci.render_target_formats.has_depth = true;
        ci.render_target_formats.depth_stencil_format = TextureFormat::TEXTURE_FORMAT_D32_SFLOAT;

        ci.shader_program.SetShader(ShaderType::PIXEL_SHADER, m_geometry_ps);
    };

    if (m_use_mesh_shader_path)
    {
        GraphicsPipelineCreateInfo mesh_ci{};
        setup_common_pipeline_state(mesh_ci);
        mesh_ci.shader_program.SetShader(ShaderType::TASK_SHADER, m_geometry_task_shader);
        mesh_ci.shader_program.SetShader(ShaderType::MESH_SHADER, m_geometry_mesh_shader);
        m_geometry_mesh_pipeline = CreateGraphicsPipeline(mesh_ci);
    }
    else
    {
        GraphicsPipelineCreateInfo static_ci{};
        setup_common_pipeline_state(static_ci);
        static_ci.vertex_input_state.attribute_count = 5;

        auto &pos = static_ci.vertex_input_state.attributes[0];
        pos.attrib_format = VertexAttribFormat::F32;
        pos.portion = 3;
        pos.binding = 0;
        pos.location = 0;
        pos.offset = 0;
        pos.stride = sizeof(Vertex);
        pos.input_rate = VertexInputRate::VERTEX_ATTRIB_RATE_VERTEX;
        pos.semantic_name = "POSITION";
        pos.semantic_index = 0;

        auto &normal = static_ci.vertex_input_state.attributes[1];
        normal.attrib_format = VertexAttribFormat::F32;
        normal.portion = 3;
        normal.binding = 0;
        normal.location = 1;
        normal.stride = sizeof(Vertex);
        normal.input_rate = VertexInputRate::VERTEX_ATTRIB_RATE_VERTEX;
        normal.offset = offsetof(Vertex, normal);
        normal.semantic_name = "NORMAL";
        normal.semantic_index = 0;

        auto &uv0 = static_ci.vertex_input_state.attributes[2];
        uv0.attrib_format = VertexAttribFormat::F32;
        uv0.portion = 2;
        uv0.binding = 0;
        uv0.location = 2;
        uv0.stride = sizeof(Vertex);
        uv0.input_rate = VertexInputRate::VERTEX_ATTRIB_RATE_VERTEX;
        uv0.offset = offsetof(Vertex, uv0);
        uv0.semantic_name = "TEXCOORD";
        uv0.semantic_index = 0;

        auto &uv1 = static_ci.vertex_input_state.attributes[3];
        uv1.attrib_format = VertexAttribFormat::F32;
        uv1.portion = 2;
        uv1.binding = 0;
        uv1.location = 3;
        uv1.stride = sizeof(Vertex);
        uv1.input_rate = VertexInputRate::VERTEX_ATTRIB_RATE_VERTEX;
        uv1.offset = offsetof(Vertex, uv1);
        uv1.semantic_name = "TEXCOORD";
        uv1.semantic_index = 1;

        auto &tangent = static_ci.vertex_input_state.attributes[4];
        tangent.attrib_format = VertexAttribFormat::F32;
        tangent.portion = 3;
        tangent.binding = 0;
        tangent.location = 4;
        tangent.stride = sizeof(Vertex);
        tangent.input_rate = VertexInputRate::VERTEX_ATTRIB_RATE_VERTEX;
        tangent.offset = offsetof(Vertex, tangent);
        tangent.semantic_name = "TANGENT";
        tangent.semantic_index = 0;

        static_ci.shader_program.SetShader(ShaderType::VERTEX_SHADER, m_geometry_static_vs);
        m_geometry_static_pipeline = CreateGraphicsPipeline(static_ci);
    }

    if (!m_use_mesh_shader_path)
    {
        GraphicsPipelineCreateInfo skinned_ci{};
        setup_common_pipeline_state(skinned_ci);
        skinned_ci.vertex_input_state.attribute_count = 7;

        auto &pos = skinned_ci.vertex_input_state.attributes[0];
        pos.attrib_format = VertexAttribFormat::F32;
        pos.portion = 3;
        pos.binding = 0;
        pos.location = 0;
        pos.offset = 0;
        pos.stride = sizeof(Vertex);
        pos.input_rate = VertexInputRate::VERTEX_ATTRIB_RATE_VERTEX;
        pos.semantic_name = "POSITION";
        pos.semantic_index = 0;

        auto &normal = skinned_ci.vertex_input_state.attributes[1];
        normal.attrib_format = VertexAttribFormat::F32;
        normal.portion = 3;
        normal.binding = 0;
        normal.location = 1;
        normal.stride = sizeof(Vertex);
        normal.input_rate = VertexInputRate::VERTEX_ATTRIB_RATE_VERTEX;
        normal.offset = offsetof(Vertex, normal);
        normal.semantic_name = "NORMAL";
        normal.semantic_index = 0;

        auto &uv0 = skinned_ci.vertex_input_state.attributes[2];
        uv0.attrib_format = VertexAttribFormat::F32;
        uv0.portion = 2;
        uv0.binding = 0;
        uv0.location = 2;
        uv0.stride = sizeof(Vertex);
        uv0.input_rate = VertexInputRate::VERTEX_ATTRIB_RATE_VERTEX;
        uv0.offset = offsetof(Vertex, uv0);
        uv0.semantic_name = "TEXCOORD";
        uv0.semantic_index = 0;

        auto &uv1 = skinned_ci.vertex_input_state.attributes[3];
        uv1.attrib_format = VertexAttribFormat::F32;
        uv1.portion = 2;
        uv1.binding = 0;
        uv1.location = 3;
        uv1.stride = sizeof(Vertex);
        uv1.input_rate = VertexInputRate::VERTEX_ATTRIB_RATE_VERTEX;
        uv1.offset = offsetof(Vertex, uv1);
        uv1.semantic_name = "TEXCOORD";
        uv1.semantic_index = 1;

        auto &tangent = skinned_ci.vertex_input_state.attributes[4];
        tangent.attrib_format = VertexAttribFormat::F32;
        tangent.portion = 3;
        tangent.binding = 0;
        tangent.location = 4;
        tangent.stride = sizeof(Vertex);
        tangent.input_rate = VertexInputRate::VERTEX_ATTRIB_RATE_VERTEX;
        tangent.offset = offsetof(Vertex, tangent);
        tangent.semantic_name = "TANGENT";
        tangent.semantic_index = 0;

        auto &joint_indices = skinned_ci.vertex_input_state.attributes[5];
        joint_indices.attrib_format = VertexAttribFormat::F32;
        joint_indices.portion = 4;
        joint_indices.binding = 0;
        joint_indices.location = 5;
        joint_indices.stride = sizeof(Vertex);
        joint_indices.input_rate = VertexInputRate::VERTEX_ATTRIB_RATE_VERTEX;
        joint_indices.offset = offsetof(Vertex, joint_indices);
        joint_indices.semantic_name = "BLENDINDICES";
        joint_indices.semantic_index = 0;

        auto &joint_weights = skinned_ci.vertex_input_state.attributes[6];
        joint_weights.attrib_format = VertexAttribFormat::F32;
        joint_weights.portion = 4;
        joint_weights.binding = 0;
        joint_weights.location = 6;
        joint_weights.stride = sizeof(Vertex);
        joint_weights.input_rate = VertexInputRate::VERTEX_ATTRIB_RATE_VERTEX;
        joint_weights.offset = offsetof(Vertex, joint_weights);
        joint_weights.semantic_name = "BLENDWEIGHT";
        joint_weights.semantic_index = 0;

        skinned_ci.shader_program.SetShader(ShaderType::VERTEX_SHADER, m_geometry_skinned_vs);
        m_geometry_skinned_pipeline = CreateGraphicsPipeline(skinned_ci);
    }

    // Create resizable render targets.
    CreateResizableRenderTarget(m_gbuffer0_rt, RenderTargetCreateInfo{RenderTargetFormat::TEXTURE_FORMAT_RGBA8_UNORM,
                                                                      RenderTargetType::COLOR, m_width, m_height});
    CreateResizableRenderTarget(m_gbuffer1_rt, RenderTargetCreateInfo{RenderTargetFormat::TEXTURE_FORMAT_RGBA8_UNORM,
                                                                      RenderTargetType::COLOR, m_width, m_height});
    CreateResizableRenderTarget(m_gbuffer2_rt,
                                RenderTargetCreateInfo{RenderTargetFormat::TEXTURE_FORMAT_R11G11B10_UFLOAT,
                                                       RenderTargetType::COLOR, m_width, m_height});
    CreateResizableRenderTarget(m_gbuffer3_rt, RenderTargetCreateInfo{RenderTargetFormat::TEXTURE_FORMAT_RGBA8_UNORM,
                                                                      RenderTargetType::COLOR, m_width, m_height});
    CreateResizableRenderTarget(m_gbuffer4_rt, RenderTargetCreateInfo{RenderTargetFormat::TEXTURE_FORMAT_RG32_SFLOAT,
                                                                      RenderTargetType::COLOR, m_width, m_height});
    CreateResizableRenderTarget(m_depth_rt, RenderTargetCreateInfo{RenderTargetFormat::TEXTURE_FORMAT_D32_SFLOAT,
                                                                   RenderTargetType::DEPTH_STENCIL, m_width, m_height});

    SetResizeCallback([this](u32 width, u32 height) {
        m_width = width;
        m_height = height;
    });

    // Create TAA buffer
    m_taa_prev_curr_offset_buffer = rhi->CreateBuffer(BufferCreateInfo{DescriptorType::DESCRIPTOR_TYPE_CONSTANT_BUFFER,
                                                                       ResourceState::RESOURCE_STATE_SHADER_RESOURCE,
                                                                       sizeof(TAARDGPass::TAAPrevCurrOffset)});
}

DeferredShadingGeometryPass::~DeferredShadingGeometryPass()
{
    DestroyShader(m_geometry_static_vs);
    DestroyShader(m_geometry_skinned_vs);
    DestroyShader(m_geometry_task_shader);
    DestroyShader(m_geometry_mesh_shader);
    DestroyShader(m_geometry_ps);
    DestroyPipeline(m_geometry_static_pipeline);
    DestroyPipeline(m_geometry_skinned_pipeline);
    DestroyPipeline(m_geometry_mesh_pipeline);
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
    begin_info.render_area = Rect{0, 0, m_width, m_height};
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

    std::vector<Texture *> material_textures;
    for (auto &tex : m_scene_manager->material_textures)
    {
        material_textures.push_back(tex);
    }

    auto setup_raster_pipeline_resources = [&](Pipeline *pipeline, bool skinned) {
        if (pipeline == nullptr)
        {
            return;
        }
        pipeline->SetResource(m_scene_manager->GetCameraBuffer(), "CameraParamsUb_cb");
        pipeline->SetResource(m_scene_manager->instance_parameter_buffer, "instance_parameter");
        pipeline->SetResource(m_scene_manager->prev_instance_model_buffer, "prev_instance_model_matrices");
        if (skinned)
        {
            pipeline->SetResource(m_scene_manager->skin_joint_matrix_buffer, "skin_joint_matrices");
            pipeline->SetResource(m_scene_manager->prev_skin_joint_matrix_buffer, "prev_skin_joint_matrices");
        }
        pipeline->SetResource(m_scene_manager->material_description_buffer, "material_descriptions");
        pipeline->SetResource(m_sampler, "default_sampler");
        pipeline->SetResource(m_taa_prev_curr_offset_buffer, "TAAOffsets_cb");
        if (!material_textures.empty())
        {
            pipeline->SetBindlessResource(material_textures, "material_textures");
        }
    };

    auto setup_mesh_pipeline_resources = [&](Pipeline *pipeline) {
        if (pipeline == nullptr)
        {
            return;
        }
        std::vector<Buffer *> vertex_buffers = m_scene_manager->vertex_buffers;
        pipeline->SetResource(m_scene_manager->GetCameraBuffer(), "CameraParamsUb_cb");
        pipeline->SetResource(m_scene_manager->instance_parameter_buffer, "instance_parameter");
        pipeline->SetResource(m_scene_manager->prev_instance_model_buffer, "prev_instance_model_matrices");
        pipeline->SetResource(m_scene_manager->skin_joint_matrix_buffer, "skin_joint_matrices");
        pipeline->SetResource(m_scene_manager->prev_skin_joint_matrix_buffer, "prev_skin_joint_matrices");
        pipeline->SetResource(m_scene_manager->material_description_buffer, "material_descriptions");
        pipeline->SetResource(m_scene_manager->GetMeshletDescBuffer(), "meshlet_descs");
        pipeline->SetResource(m_scene_manager->GetMeshletVertexIndexBuffer(), "meshlet_vertex_indices");
        pipeline->SetResource(m_scene_manager->GetMeshletTriangleBuffer(), "meshlet_triangle_indices");
        pipeline->SetResource(m_sampler, "default_sampler");
        pipeline->SetResource(m_taa_prev_curr_offset_buffer, "TAAOffsets_cb");
        if (!vertex_buffers.empty())
        {
            pipeline->SetBindlessResource(vertex_buffers, "vertex_buffers");
        }
        if (!material_textures.empty())
        {
            pipeline->SetBindlessResource(material_textures, "material_textures");
        }
    };

    if (m_use_mesh_shader_path)
    {
        setup_mesh_pipeline_resources(m_geometry_mesh_pipeline);
    }
    else
    {
        setup_raster_pipeline_resources(m_geometry_static_pipeline, false);
        setup_raster_pipeline_resources(m_geometry_skinned_pipeline, true);
    }

    cl->BeginRenderPass(begin_info);

    if (m_use_mesh_shader_path)
    {
        constexpr u32 k_max_mesh_tasks_per_draw = 65535;
        cl->BindPipeline(m_geometry_mesh_pipeline);
        u32 remaining = static_cast<u32>(m_scene_manager->meshlet_descs.size());
        u32 meshlet_offset = 0;
        while (remaining > 0)
        {
            const u32 batch = std::min(k_max_mesh_tasks_per_draw, remaining);
            cl->BindPushConstant(m_geometry_mesh_pipeline, "meshlet_draw_offset", &meshlet_offset);
            cl->DrawMeshTasks(batch, 1, 1);
            meshlet_offset += batch;
            remaining -= batch;
        }
    }
    else
    {
        for (u32 mesh_data = 0; mesh_data < m_scene_manager->mesh_data.size(); mesh_data++)
        {
            auto &mesh = m_scene_manager->mesh_data[mesh_data];
            auto ib = m_scene_manager->index_buffers[mesh.index_buffer_offset];
            auto vb = m_scene_manager->vertex_buffers[mesh.vertex_buffer_offset];

            u32 command_index = mesh.draw_offset;
            const u32 command_end = mesh.draw_offset + mesh.draw_count;
            while (command_index < command_end)
            {
                const auto &first_command = m_scene_manager->scene_indirect_draw_command1[command_index];
                const bool first_is_skinned =
                    m_scene_manager->instance_params[first_command.mesh_id_offset].skinning_enabled != 0;

                u32 batch_end = command_index + 1;
                while (batch_end < command_end)
                {
                    const auto &cmd = m_scene_manager->scene_indirect_draw_command1[batch_end];
                    const bool is_skinned = m_scene_manager->instance_params[cmd.mesh_id_offset].skinning_enabled != 0;
                    if (is_skinned != first_is_skinned)
                    {
                        break;
                    }
                    ++batch_end;
                }

                Pipeline *pipeline = first_is_skinned ? m_geometry_skinned_pipeline : m_geometry_static_pipeline;
                cl->BindPipeline(pipeline);
                u32 offset = 0;
                cl->BindVertexBuffers(1, &vb, &offset);
                cl->BindIndexBuffer(ib, 0);

                u32 mesh_id_offset = first_command.mesh_id_offset;
                cl->BindPushConstant(pipeline, "mesh_draw_offset", &mesh_id_offset);
                cl->DrawIndirectIndexedInstanced(m_scene_manager->indirect_draw_command_buffer1,
                                                 sizeof(DX12DrawIndexedInstancedCommand) * command_index,
                                                 batch_end - command_index, sizeof(DX12DrawIndexedInstancedCommand));

                command_index = batch_end;
            }
        }
    }

    cl->EndRenderPass();
}
