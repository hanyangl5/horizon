
#include <runtime/core/log/Log.h>
#include <runtime/function/rhi/RHI.h>
#include <runtime/function/rhi/RHIUtils.h>
#include <runtime/interface/HorizonRuntime.h>
#include <runtime/system/render/RenderSystem.h>

namespace TEST {

using namespace Horizon;
using namespace Horizon::Backend;

class RHITest {
  public:
    RHITest() {

        Horizon::Memory::initialize();
        HorizonConfig config{};
        config.width = 800;
        config.height = 600;
        // config.asset_path =
        config.render_backend = RenderBackend::RENDER_BACKEND_VULKAN;
        config.offscreen = false;
        engine = Memory::Alloc<HorizonRuntime>(config);

        width = config.width;
        height = config.height;
    }
    ~RHITest() { Horizon::Memory::destroy(); }
    HorizonRuntime *engine{};
    u32 width, height;
};

void CommandListTest(RHITest *rhi_test) {
    auto *rhi = rhi_test->engine->m_render_system->GetRhi();

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

    Container::Array<CommandList *> cmdlists{cmdlist};
    QueueSubmitInfo submit_info{};
    submit_info.queue_type = CommandQueueType::TRANSFER;
    submit_info.command_lists.swap(cmdlists);
    rhi->SubmitCommandLists(submit_info);

    rhi->WaitGpuExecution(CommandQueueType::TRANSFER);
}

} // namespace TEST

int main() {
    TEST::RHITest rhi_test;
    TEST::CommandListTest(&rhi_test);
}