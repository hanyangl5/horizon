#pragma once
#include <chrono>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_set>

#include <iniparser.h>

#include <argparse/argparse.hpp>

#include <runtime/core/log/Log.h>
#include <runtime/core/utils/renderdoc/RenderDoc.h>
#include <runtime/core/window/Window.h>

#include <runtime/function/rhi/RHI.h>
#include <runtime/function/rhi/RHIUtils.h>
#include <runtime/function/scene/geometry/mesh/Mesh.h>
#include <runtime/function/scene/light/Light.h>

#include <runtime/interface/Engine.h>

#include <runtime/system/input/InputSystem.h>
#include <runtime/system/render/RenderSystem.h>

using namespace Horizon;
using namespace Horizon::RHI;

class Camera {


};

class Pbr {
  public:
    Pbr() {
        EngineConfig config{};
        config.width = 800;
        config.height = 600;
        config.render_backend = RenderBackend::RENDER_BACKEND_VULKAN;
        config.offscreen = false;
        engine = std::make_unique<Engine>(config);

        width = config.width;
        height = config.height;
    }

  public:
    std::unique_ptr<Engine> engine{};
    std::string asset_path = "C:/FILES/horizon/horizon/assets/";
    u32 width, height;

    void run() {



        auto rhi = engine->m_render_system->GetRhi();
        std::string vs_path = asset_path + "shaders/pbr.vert.hsl";
        std::string ps_path = asset_path + "shaders/pbr.frag.hsl";

        auto vs = rhi->CreateShader(ShaderType::VERTEX_SHADER, 0, vs_path);

        auto ps = rhi->CreateShader(ShaderType::PIXEL_SHADER, 0, ps_path);

        auto rt0 = rhi->CreateRenderTarget(RenderTargetCreateInfo{RenderTargetFormat::TEXTURE_FORMAT_RGBA8_UNORM,
                                                                  RenderTargetType::COLOR, width, height});
        auto depth = rhi->CreateRenderTarget(RenderTargetCreateInfo{RenderTargetFormat::TEXTURE_FORMAT_D32_SFLOAT,
                                                                    RenderTargetType::DEPTH_STENCIL, width, height});

        GraphicsPipelineCreateInfo info{};

        info.vertex_input_state.attribute_count = 4;

        auto &pos = info.vertex_input_state.attributes[0];
        pos.attrib_format = VertexAttribFormat::F32; // position
        pos.portion = 3;
        pos.binding = 0;
        pos.location = 0;
        pos.offset = 0;
        pos.input_rate = VertexInputRate::VERTEX_ATTRIB_RATE_VERTEX;

        auto &normal = info.vertex_input_state.attributes[1];
        normal.attrib_format = VertexAttribFormat::F32; // normal, TOOD: SN16 is a better format
        normal.portion = 3;
        normal.binding = 0;
        normal.location = 1;
        normal.input_rate = VertexInputRate::VERTEX_ATTRIB_RATE_VERTEX;
        normal.offset = offsetof(Vertex, normal);

        auto &uv0 = info.vertex_input_state.attributes[2];
        uv0.attrib_format = VertexAttribFormat::F32; // uv0 TOOD: UN16 is a better format
        uv0.portion = 2;
        uv0.binding = 0;
        uv0.location = 2;
        uv0.input_rate = VertexInputRate::VERTEX_ATTRIB_RATE_VERTEX;
        uv0.offset = offsetof(Vertex, uv0);

        auto &uv1 = info.vertex_input_state.attributes[3];
        uv1.attrib_format = VertexAttribFormat::F32; // uv1 TOOD: UN16 is a better format
        uv1.portion = 2;
        uv1.binding = 0;
        uv1.location = 3;
        uv1.input_rate = VertexInputRate::VERTEX_ATTRIB_RATE_VERTEX;
        uv1.offset = offsetof(Vertex, uv1);

        info.view_port_state.width = width;
        info.view_port_state.height = height;

        info.depth_stencil_state.depth_func = DepthFunc::LESS;
        info.depth_stencil_state.depthNear = 0.0f;
        info.depth_stencil_state.depthNear = 1.0f;
        info.depth_stencil_state.depth_test = true;
        info.depth_stencil_state.depth_write = true;
        info.depth_stencil_state.stencil_enabled = false;

        info.input_assembly_state.topology = PrimitiveTopology::TRIANGLE_LIST;

        info.multi_sample_state.sample_count = 1;

        info.rasterization_state.cull_mode = CullMode::BACK;
        info.rasterization_state.discard = false;
        info.rasterization_state.fill_mode = FillMode::TRIANGLE;
        info.rasterization_state.front_face = FrontFace::CCW;

        info.render_target_formats.color_attachment_count = 1;
        info.render_target_formats.color_attachment_formats =
            std::vector<TextureFormat>{TextureFormat::TEXTURE_FORMAT_RGBA8_UNORM};
        info.render_target_formats.has_depth = true;
        info.render_target_formats.depth_stencil_format = TextureFormat::TEXTURE_FORMAT_D32_SFLOAT;

        auto pipeline = rhi->CreateGraphicsPipeline(info);

        Mesh mesh(MeshDesc{VertexAttributeType::POSTION | VertexAttributeType::NORMAL | VertexAttributeType::UV0});
        //mesh.LoadMesh(asset_path + "models/DamagedHelmet/DamagedHelmet.gltf");
        mesh.LoadMesh(asset_path + "models/FlightHelmet/glTF/FlightHelmet.gltf");
        //mesh.LoadMesh(asset_path + "models/Sponza/glTF/Sponza.gltf");
        mesh.CreateGpuResources(rhi);

        SamplerDesc sampler_desc{};
        sampler_desc.min_filter = FilterType::FILTER_LINEAR;
        sampler_desc.mag_filter = FilterType::FILTER_LINEAR;
        sampler_desc.mip_map_mode = MipMapMode::MIPMAP_MODE_LINEAR;
        sampler_desc.address_u = AddressMode::ADDRESS_MODE_REPEAT;
        sampler_desc.address_v = AddressMode::ADDRESS_MODE_REPEAT;
        sampler_desc.address_w = AddressMode::ADDRESS_MODE_REPEAT;

        auto sampler = rhi->GetSampler(sampler_desc);
        auto view = Math::LookAt(Math::float3(0.0f, 0.0f, 1.0f), Math::float3(0.0f, 0.0f, 0.0f),
                                 Math::float3(0.0f, 1.0f, 0.0f));

        auto projection = Math::Perspective(90.0f, (float)width / (float)height, 0.1f, 100.0f);


        struct CameraUb {
            Math::float4x4 vp;
            Math::float3 camera_pos;
        } camera_ub;

        camera_ub.vp = view * projection;
        camera_ub.vp = camera_ub.vp.Transpose();
        camera_ub.camera_pos = Math::float3(0.0, 0.0, 1.0);

        auto camera_buffer =
            rhi->CreateBuffer(BufferCreateInfo{DescriptorType::DESCRIPTOR_TYPE_CONSTANT_BUFFER,
                                               ResourceState::RESOURCE_STATE_SHADER_RESOURCE, sizeof(CameraUb)});

        u32 light_count = 1;

        auto light_count_buffer =
            rhi->CreateBuffer(BufferCreateInfo{DescriptorType::DESCRIPTOR_TYPE_CONSTANT_BUFFER,
                                               ResourceState::RESOURCE_STATE_SHADER_RESOURCE, 4 * sizeof(u32)});
        Light *directional_light = new DirectionalLight(Math::color(1.0, 1.0, 1.0), 1.0, Math::float3(0.0, 0.0, -1.0));

        auto light_buffer = rhi->CreateBuffer(BufferCreateInfo{DescriptorType::DESCRIPTOR_TYPE_CONSTANT_BUFFER,
                                                               ResourceState::RESOURCE_STATE_SHADER_RESOURCE,
                                                               sizeof(directional_light->params)});


        //auto image_acquired_semaphore = rhi->GetSemaphore();
        auto resource_uploaded_semaphore = rhi->GetSemaphore();
        bool resources_uploaded = false;

        for (u32 frame = 0; frame < 1; frame++) {

            engine->BeginNewFrame();
            Horizon::RDC::StartFrameCapture();


            pipeline->SetGraphicsShader(vs, ps);

            auto per_frame_descriptor_set = pipeline->GetDescriptorSet(ResourceUpdateFrequency::PER_FRAME);

            auto transfer = rhi->GetCommandList(CommandQueueType::TRANSFER);

            transfer->BeginRecording();
            transfer->UpdateBuffer(light_buffer.get(), &directional_light->params, sizeof(LightParams));
            transfer->UpdateBuffer(light_count_buffer.get(), &light_count, sizeof(light_count) * 4);
            transfer->UpdateBuffer(camera_buffer.get(), &camera_ub, sizeof(CameraUb));

            if (!resources_uploaded) {
                resources_uploaded = true;
                mesh.UploadResources(transfer);
            }

            transfer->EndRecording();

            {
                QueueSubmitInfo submit_info{};
                submit_info.queue_type = CommandQueueType::TRANSFER;
                submit_info.command_lists.push_back(transfer);
                submit_info.signal_semaphores.push_back(resource_uploaded_semaphore.get());
                rhi->SubmitCommandLists(submit_info);
            }

            // perframe descriptor set
            per_frame_descriptor_set->SetResource(camera_buffer.get(), 0);
            per_frame_descriptor_set->SetResource(light_buffer.get(), 2);
            per_frame_descriptor_set->SetResource(light_count_buffer.get(), 1);
            per_frame_descriptor_set->Update();

            // material descriptor set
            for (auto &node : mesh.GetNodes()) {
                if (node.mesh_primitives.empty()) {
                    continue;
                }
                std::unordered_set<Material *> materials{};
                for (const auto &m : node.mesh_primitives) {
                    materials.emplace(&mesh.GetMaterial(m->material_id));
                }

                for (auto &material : materials) {
                    material->material_descriptor_set = pipeline->GetDescriptorSet(ResourceUpdateFrequency::PER_BATCH);

                    // material.material_params.param_bitmask;
                    // if (material->material_params.param_bitmask & HAS_BASE_COLOR) {
                    material->material_descriptor_set->SetResource(
                        material->material_textures.at(MaterialTextureType::BASE_COLOR).texture.get(), 0);
                    //}
                    // if (material->material_params.param_bitmask & HAS_NORMAL) {
                    material->material_descriptor_set->SetResource(
                        material->material_textures.at(MaterialTextureType::NORMAL).texture.get(), 1);
                    //}
                    // if (material->material_params.param_bitmask & HAS_METALLIC_ROUGHNESS) {
                    material->material_descriptor_set->SetResource(
                        material->material_textures.at(MaterialTextureType::METALLIC_ROUGHTNESS).texture.get(), 2);
                    //}
                    material->material_descriptor_set->SetResource(sampler.get(), 3);
                    material->material_descriptor_set->SetResource(material->param_buffer.get(), 4);
                    material->material_descriptor_set->Update();
                }
            }

            auto cl = rhi->GetCommandList(CommandQueueType::GRAPHICS);
            cl->BeginRecording();

            RenderPassBeginInfo begin_info{};
            begin_info.render_area = Rect{0, 0, width, height};
            begin_info.render_targets[0].data = rt0.get();
            begin_info.render_targets[0].clear_color = ClearValueColor{Math::float4(0.0)};
            begin_info.depth_stencil.data = depth.get();
            begin_info.depth_stencil.clear_color = ClearValueDepthStencil{1.0, 0};

            cl->BeginRenderPass(begin_info);
            cl->BindPipeline(pipeline);

            u32 offset = 0;
            auto vb = mesh.GetVertexBuffer();
            cl->BindVertexBuffers(1, &vb, &offset);
            cl->BindIndexBuffer(mesh.GetIndexBuffer(), 0);

            cl->BindDescriptorSets(pipeline, per_frame_descriptor_set.get());

            for (auto &node : mesh.GetNodes()) {
                if (node.mesh_primitives.empty()) {
                    continue;
                }
                auto mat = node.GetModelMatrix().Transpose();
                cl->BindPushConstant(pipeline, "model_matrix", &mat);

                for (auto &m : node.mesh_primitives) {
                    auto &material = mesh.GetMaterial(m->material_id);
                    cl->BindDescriptorSets(pipeline, material.material_descriptor_set.get());
                    cl->DrawIndexedInstanced(m->index_count, m->index_offset, 0);
                }
            }

            cl->EndRenderPass();

            cl->EndRecording();

            auto render_complete_semaphore = rhi->GetSemaphore();
            {
                QueueSubmitInfo submit_info{};
                submit_info.queue_type = CommandQueueType::GRAPHICS;
                submit_info.command_lists.push_back(cl);
                submit_info.wait_semaphores.push_back(resource_uploaded_semaphore.get());
                submit_info.signal_semaphores.push_back(render_complete_semaphore.get());
                rhi->SubmitCommandLists(submit_info);
            }

            //{
            //    QueuePresentInfo info{};
            //    info.wait_semaphores.push_back(render_complete_semaphore.get());

            //    rhi->Present(info);
            //}
            Horizon::RDC::EndFrameCapture();
            engine->EndFrame();
        }
        rhi->DestroyPipeline(pipeline);
        rhi->DestroyShader(vs);
        rhi->DestroyShader(ps);
        LOG_INFO("draw done");
    }
};



int main() { Pbr pbr;
    pbr.run();
}