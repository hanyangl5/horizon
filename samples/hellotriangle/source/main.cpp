#include "config.hpp"
#include <app_framework/app_framework.h>
#include <core/log.h>
#include <core/math.h>
#include <core/path.h>
#include <rhi/buffer.h>
#include <rhi/command_list.h>
#include <rhi/enums.h>
#include <rhi/pipeline.h>
#include <rhi/resource_barrier.h>
#include <rhi/rhi.h>
#include <rhi/shader.h>
#include <rhi/swap_chain.h>

#include <array>
#include <chrono>
#include <memory>
#include <thread>

Horizon::Path shader_dir;
using namespace Horizon;
using namespace Horizon::Backend;

struct TestVertex
{
    Math::float3 position;
    Math::float3 color;
};

class HelloTriangleApp : public AppFramework
{
  public:
    HelloTriangleApp() : AppFramework("Hello Triangle", 800, 600)
    {
        Horizon::Path::set_project_root(RUNTIME_SAMPLE_ROOT);
    }

  protected:
    void Initialize() override
    {
        Horizon::Path::resolve_resource_paths(&shader_dir);
        auto rhi = GetRhi();
        if (!rhi)
        {
            LOG_ERROR("RHI is null");
            return;
        }

        // Create swap chain
        SwapChainCreateInfo swap_chain_info{};
        swap_chain_info.back_buffer_count = 2;
        m_swap_chain = rhi->CreateSwapChain(swap_chain_info);

        // Create shaders
        m_vs_shader = rhi->CreateShader(ShaderType::VERTEX_SHADER, shader_dir / "triangle.hlsl", "VSMain");
        m_ps_shader = rhi->CreateShader(ShaderType::PIXEL_SHADER, shader_dir / "triangle.hlsl", "PSMain");

        // Create vertex buffer
        TestVertex vertices[] = {
            {{0.0f, 0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},  // Top vertex - Red
            {{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}}, // Bottom right - Green
            {{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}} // Bottom left - Blue
        };

        BufferCreateInfo vertex_buffer_info{};
        vertex_buffer_info.size = sizeof(vertices);
        vertex_buffer_info.descriptor_types = (DescriptorTypes)DescriptorType::DESCRIPTOR_TYPE_VERTEX_BUFFER;
        vertex_buffer_info.initial_state = ResourceState::RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
        vertex_buffer_info.debug_name = "TriangleVertexBuffer";
        m_vertex_buffer = rhi->CreateBuffer(vertex_buffer_info);

        // Upload vertex data using command list
        CommandList *upload_cmd_list = rhi->GetCommandList(CommandQueueType::GRAPHICS);
        upload_cmd_list->BeginRecording();
        upload_cmd_list->UpdateBuffer(m_vertex_buffer, vertices, sizeof(vertices));
        upload_cmd_list->EndRecording();

        QueueSubmitInfo upload_submit_info{};
        upload_submit_info.queue_type = CommandQueueType::GRAPHICS;
        upload_submit_info.command_lists.push_back(upload_cmd_list);
        rhi->SubmitCommandLists(upload_submit_info);
        rhi->WaitGpuExecution(CommandQueueType::GRAPHICS);

        // Define vertex input layout
        VertexInputState vertex_input_state{};
        vertex_input_state.attribute_count = 2;
        vertex_input_state.attributes[0] = VertexAttributeDescription{
            VertexAttribFormat::F32,
            3, // float3
            VertexInputRate::VERTEX_ATTRIB_RATE_VERTEX,
            0,
            0,
            sizeof(TestVertex),
            0,
            "POSITION",
            0,
        };
        vertex_input_state.attributes[1] = VertexAttributeDescription{
            VertexAttribFormat::F32,
            3, // float3
            VertexInputRate::VERTEX_ATTRIB_RATE_VERTEX,
            1,
            0,
            sizeof(TestVertex),
            12,
            "COLOR",
            0,
        };

        GraphicsPipelineCreateInfo pipeline_info{};
        pipeline_info.shader_program.SetShader(ShaderType::VERTEX_SHADER, m_vs_shader);
        pipeline_info.shader_program.SetShader(ShaderType::PIXEL_SHADER, m_ps_shader);
        pipeline_info.vertex_input_state = vertex_input_state;
        pipeline_info.input_assembly_state.topology = PrimitiveTopology::TRIANGLE_LIST;

        pipeline_info.view_port_state.width = GetWidth();
        pipeline_info.view_port_state.height = GetHeight();

        pipeline_info.multi_sample_state.sample_count = 1;
        pipeline_info.rasterization_state.cull_mode = CullMode::NONE;
        pipeline_info.rasterization_state.front_face = FrontFace::CCW;
        pipeline_info.rasterization_state.fill_mode = FillMode::TRIANGLE;
        pipeline_info.depth_stencil_state.depth_test = false;
        pipeline_info.depth_stencil_state.depth_write = false;
        pipeline_info.render_target_formats.color_attachment_count = 1;
        pipeline_info.render_target_formats.color_attachment_formats.resize(1);
        pipeline_info.render_target_formats.color_attachment_formats[0] = TextureFormat::TEXTURE_FORMAT_RGBA8_UNORM;
        pipeline_info.render_target_formats.depth_stencil_format = TextureFormat::TEXTURE_FORMAT_D32_SFLOAT;
        pipeline_info.rasterization_state.discard = false;

        m_pipeline = rhi->CreateGraphicsPipeline(pipeline_info);
    }

    void RenderLoop() override
    {
        auto rhi = GetRhi();
        if (!rhi || !m_swap_chain)
        {
            return;
        }

        // Acquire next frame
        rhi->AcquireNextFrame(m_swap_chain);

        // Get command list
        CommandList *cmd_list = rhi->GetCommandList(CommandQueueType::GRAPHICS);
        cmd_list->BeginRecording();

        // Get current render target
        RenderTarget *render_target = m_swap_chain->GetRenderTarget();
        Texture *render_target_texture = render_target->GetTexture();

        // Transition render target to RENDER_TARGET
        BarrierDesc barrier{};
        TextureBarrierDesc rt_barrier{};
        rt_barrier.texture = render_target_texture;
        rt_barrier.src_state = ResourceState::RESOURCE_STATE_UNDEFINED;
        rt_barrier.dst_state = ResourceState::RESOURCE_STATE_RENDER_TARGET;
        rt_barrier.first_mip_level = 0;
        rt_barrier.mip_level_count = 1;
        rt_barrier.first_layer = 0;
        rt_barrier.layer_count = 1;
        rt_barrier.queue = CommandQueueType::GRAPHICS;
        rt_barrier.queue_op = QueueOp::IGNORED;
        barrier.texture_memory_barriers.push_back(rt_barrier);
        cmd_list->InsertBarrier(barrier);

        // Begin render pass
        RenderPassBeginInfo render_pass_info{};
        render_pass_info.render_target_count = 1;
        render_pass_info.render_targets[0].data = render_target;
        render_pass_info.render_targets[0].load_op = RenderTargetLoadOp::CLEAR;
        render_pass_info.render_targets[0].store_op = RenderTargetStoreOp::STORE;
        ClearColorValue clear_color{};
        clear_color.float32[0] = 0.1f;
        clear_color.float32[1] = 0.1f;
        clear_color.float32[2] = 0.1f;
        clear_color.float32[3] = 1.0f;
        render_pass_info.render_targets[0].clear_color = clear_color;
        render_pass_info.render_area.x = 0;
        render_pass_info.render_area.y = 0;
        render_pass_info.render_area.w = GetWidth();
        render_pass_info.render_area.h = GetHeight();
        cmd_list->BeginRenderPass(render_pass_info);

        // Bind pipeline
        cmd_list->BindPipeline(m_pipeline);

        // Bind vertex buffer
        Buffer *vertex_buffers[] = {m_vertex_buffer};
        u32 offsets[] = {0};
        cmd_list->BindVertexBuffers(1, vertex_buffers, offsets);

        // Draw triangle
        cmd_list->DrawInstanced(3, 0, 1, 0);

        // End render pass
        cmd_list->EndRenderPass();

        // Transition render target from RENDER_TARGET to PRESENT
        BarrierDesc present_barrier{};
        TextureBarrierDesc present_rt_barrier{};
        present_rt_barrier.texture = render_target_texture;
        present_rt_barrier.src_state = ResourceState::RESOURCE_STATE_RENDER_TARGET;
        present_rt_barrier.dst_state = ResourceState::RESOURCE_STATE_PRESENT;
        present_rt_barrier.first_mip_level = 0;
        present_rt_barrier.mip_level_count = 1;
        present_rt_barrier.first_layer = 0;
        present_rt_barrier.layer_count = 1;
        present_rt_barrier.queue = CommandQueueType::GRAPHICS;
        present_rt_barrier.queue_op = QueueOp::IGNORED;
        present_barrier.texture_memory_barriers.push_back(present_rt_barrier);
        cmd_list->InsertBarrier(present_barrier);

        // End command list
        cmd_list->EndRecording();

        // Submit command list
        QueueSubmitInfo submit_info{};
        submit_info.queue_type = CommandQueueType::GRAPHICS;
        submit_info.command_lists.push_back(cmd_list);
        submit_info.wait_image_acquired = true;
        submit_info.signal_render_complete = true;
        rhi->SubmitCommandLists(submit_info);

        // Present
        QueuePresentInfo present_info{};
        present_info.swap_chain = m_swap_chain;
        rhi->Present(present_info);

        // Wait for GPU
        rhi->WaitGpuExecution(CommandQueueType::GRAPHICS);
    }

    void Cleanup() override
    {
        auto rhi = GetRhi();
        if (rhi)
        {
            if (m_pipeline)
            {
                rhi->DestroyPipeline(m_pipeline);
                m_pipeline = nullptr;
            }
            if (m_vertex_buffer)
            {
                rhi->DestroyBuffer(m_vertex_buffer);
                m_vertex_buffer = nullptr;
            }
            if (m_vs_shader)
            {
                rhi->DestroyShader(m_vs_shader);
                m_vs_shader = nullptr;
            }
            if (m_ps_shader)
            {
                rhi->DestroyShader(m_ps_shader);
                m_ps_shader = nullptr;
            }
            if (m_swap_chain)
            {
                rhi->DestroySwapChain(m_swap_chain);
                m_swap_chain = nullptr;
            }
        }
    }

  private:
    SwapChain *m_swap_chain{nullptr};
    Shader *m_vs_shader{nullptr};
    Shader *m_ps_shader{nullptr};
    Buffer *m_vertex_buffer{nullptr};
    Pipeline *m_pipeline{nullptr};
};

DEFINE_HORIZON_APP(HelloTriangle)
