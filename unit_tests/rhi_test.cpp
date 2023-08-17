
#include <runtime/core/log/Log.h>
#include <runtime/render/Render.h>

namespace TEST {

using namespace Horizon;
using namespace Horizon::Backend;

class RHITest {
  public:
    RHITest() {
        Config config{};
        config.width = width;
        config.height = height;
        config.render_backend = RenderBackend::RENDER_BACKEND_VULKAN;
        config.app_type = Horizon::ApplicationType::OFFSCREEN_GRAPHICS;
        renderer = std::make_unique<Renderer>(config);

        width = config.width;
        height = config.height;
    }
    ~RHITest() = default;
    u32 width = 800;
    u32 height = 600;
    std::unique_ptr<Window> window;
    std::unique_ptr<Renderer> renderer;
};

void CommandListTest(RHITest *rhi_test) {
    auto *rhi = rhi_test->renderer->GetRhi();

    CommandList *cmdlist = nullptr;

    Buffer *buffer =
        rhi->CreateBuffer(BufferCreateInfo{DescriptorType::DESCRIPTOR_TYPE_CONSTANT_BUFFER,
                                           ResourceState::RESOURCE_STATE_SHADER_RESOURCE, sizeof(Math::float3)});

    Math::float3 data{static_cast<float>(rand()) / static_cast<float>(RAND_MAX),
                      static_cast<float>(rand()) / static_cast<float>(RAND_MAX),
                      static_cast<float>(rand()) / static_cast<float>(RAND_MAX)};

    auto *transfer = rhi->GetCommandList(CommandQueueType::TRANSFER);

    transfer->BeginRecording();

    // cpu -> stage
    transfer->UpdateBuffer(buffer, &data,
                           sizeof(data)); // TODO(hyl5): multithread

    transfer->EndRecording();
    // stage -> gpu
    cmdlist = transfer;

    LOG_INFO("all task done");

    std::vector<CommandList *> cmdlists{cmdlist};
    QueueSubmitInfo submit_info{};
    submit_info.queue_type = CommandQueueType::TRANSFER;
    submit_info.command_lists.swap(cmdlists);
    rhi->SubmitCommandLists(submit_info);

    rhi->WaitGpuExecution(CommandQueueType::TRANSFER);

    rhi->DestroyBuffer(buffer);
}

} // namespace TEST

int main() {
    TEST::RHITest rhi_test;
    TEST::CommandListTest(&rhi_test);
}