#include <core/log.h>
#include <core/path.h>
#include <rhi/buffer.h>
#include <rhi/command_list.h>
#include <rhi/enums.h>
#include <rhi/pipeline.h>
#include <rhi/resource_barrier.h>
#include <rhi/rhi.h>
#include <rhi/shader.h>
#include <rhi/swap_chain.h>
#include <scene/scene_renderer/config.h>
#include <scene/scene_renderer/renderer.h>

#include <GLFW/glfw3.h>
#include <memory>

Horizon::Path shader_dir = SHADER_DIR;
using namespace Horizon;
using namespace Horizon::Backend;
u32 width = 800, height = 600;
bool enable_vsync = true; // Set to false to disable vsync

struct TestVertex
{
    float position[3];
    float color[3];
};

int main()
{
    // Initialize window and renderer
    Horizon::Config config{};
    config.width = width;
    config.height = height;
    config.render_backend = RenderBackend::RENDER_BACKEND_DX12; // Use DX12 backend
    config.app_type = Horizon::ApplicationType::GRAPHICS;

    auto window = std::make_unique<Horizon::Window>("Hello Triangle - DX12", config.width, config.height);
    config.window = window.get();

    auto renderer = std::make_unique<Horizon::Renderer>(config);
    auto rhi = renderer->GetRhi();

    // Create swap chain with vsync option
    SwapChainCreateInfo swap_chain_info{};
    swap_chain_info.back_buffer_count = 2;
    SwapChain *swap_chain = rhi->CreateSwapChain(swap_chain_info);

    // Create shaders
    auto vs_shader = rhi->CreateShader(ShaderType::VERTEX_SHADER, shader_dir / "triangle.hlsl", "VSMain");
    auto ps_shader = rhi->CreateShader(ShaderType::PIXEL_SHADER, shader_dir / "triangle.hlsl", "PSMain");

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
    Buffer *vertex_buffer = rhi->CreateBuffer(vertex_buffer_info);

    // Upload vertex data using command list
    CommandList *upload_cmd_list = rhi->GetCommandList(CommandQueueType::GRAPHICS);
    upload_cmd_list->BeginRecording();
    upload_cmd_list->UpdateBuffer(vertex_buffer, vertices, sizeof(vertices));
    upload_cmd_list->EndRecording();

    QueueSubmitInfo upload_submit_info{};
    upload_submit_info.queue_type = CommandQueueType::GRAPHICS;
    upload_submit_info.command_lists.push_back(upload_cmd_list);
    rhi->SubmitCommandLists(upload_submit_info);
    rhi->WaitGpuExecution(CommandQueueType::GRAPHICS);

    // Define vertex input layout
    VertexInputState vertex_input_state{};
    vertex_input_state.attribute_count = 1;
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
        0,
        0,
        sizeof(TestVertex),
        12,
        "COLOR0",
        0,
    };

    GraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.shader_program.SetShader(ShaderType::VERTEX_SHADER, vs_shader);
    pipeline_info.shader_program.SetShader(ShaderType::PIXEL_SHADER, ps_shader);
    pipeline_info.vertex_input_state = vertex_input_state;
    pipeline_info.input_assembly_state.topology = PrimitiveTopology::TRIANGLE_LIST;

    pipeline_info.view_port_state.width = width;
    pipeline_info.view_port_state.height = height;

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

    Pipeline *pipeline = rhi->CreateGraphicsPipeline(pipeline_info);

    LOG_INFO("Hello Triangle initialized. Starting render loop...");

    // Main render loop
    while (!window->ShouldClose())
    {

        window->ProcessEvents();
        // Acquire next frame
        rhi->AcquireNextFrame(swap_chain);

        // Get command list
        CommandList *cmd_list = rhi->GetCommandList(CommandQueueType::GRAPHICS);
        cmd_list->BeginRecording();

        // Get current render target
        RenderTarget *render_target = swap_chain->GetRenderTarget();
        Texture *render_target_texture = render_target->GetTexture();

        // Transition render target to RENDER_TARGET
        // For first frame, image may be UNDEFINED, so we use UNDEFINED as src_state
        // In Vulkan, UNDEFINED->any layout transition is always valid
        BarrierDesc barrier{};
        TextureBarrierDesc rt_barrier{};
        rt_barrier.texture = render_target_texture;
        // Use UNDEFINED as src_state - this works for both first-time use (UNDEFINED)
        // and subsequent uses (will be PRESENT_SRC_KHR after first frame)
        // UNDEFINED->COLOR_ATTACHMENT_OPTIMAL is always valid in Vulkan
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
        render_pass_info.render_area.w = width;
        render_pass_info.render_area.h = height;
        cmd_list->BeginRenderPass(render_pass_info);

        // Bind pipeline
        cmd_list->BindPipeline(pipeline);

        // Bind vertex buffer
        Buffer *vertex_buffers[] = {vertex_buffer};
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
        present_info.swap_chain = swap_chain;
        rhi->Present(present_info);

        // Wait for GPU
        rhi->WaitGpuExecution(CommandQueueType::GRAPHICS);
    }

    // Cleanup
    rhi->DestroyPipeline(pipeline);
    rhi->DestroyBuffer(vertex_buffer);
    rhi->DestroyShader(vs_shader);
    rhi->DestroyShader(ps_shader);
    rhi->DestroySwapChain(swap_chain);

    LOG_INFO("Hello Triangle finished.");

    return 0;
}
