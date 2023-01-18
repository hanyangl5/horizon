// #include "config.h"

// #include <doctest/doctest.h>

// namespace TEST {

// using namespace Horizon;
// using namespace Horizon::Backend;

// class RHITest {
//   public:
//     RHITest() {
//         EngineConfig config{};
//         config.width = 800;
//         config.height = 600;
//         // config.asset_path =
//         config.render_backend = RenderBackend::RENDER_BACKEND_VULKAN;
//         config.offscreen = false;
//         engine = Memory::Alloc<Engine>(config);

//         width = config.width;
//         height = config.height;
//     }

//   public:
//     Engine* engine{};
//     Container::String asset_path = "C:/FILES/horizon/horizon/assets/";
//     u32 width, height;
// };

// TEST_CASE_FIXTURE(RHITest, "multi thread command list recording") {
//     auto &tp = engine->tp;
//     auto rhi = engine->m_render_system->GetRhi();
//     constexpr u32 cmdlist_count = 20;
//     Container::Array<CommandList *> cmdlists(cmdlist_count);
//     Container::Array<std::future<void>> results(cmdlist_count);

//     Container::Array<Buffer*> buffers;

//     for (u32 i = 0; i < cmdlist_count; i++) {
//         buffers.emplace_back(
//             rhi->CreateBuffer(BufferCreateInfo{DescriptorType::DESCRIPTOR_TYPE_CONSTANT_BUFFER,
//                                                ResourceState::RESOURCE_STATE_SHADER_RESOURCE, sizeof(math::Vector3f)}));

//         results[i] = std::move(tp->submit([&rhi, &cmdlists, &buffers, i]() {
//             math::Vector3f data{static_cast<float>(rand()) / static_cast<float>(RAND_MAX),
//                               static_cast<float>(rand()) / static_cast<float>(RAND_MAX),
//                               static_cast<float>(rand()) / static_cast<float>(RAND_MAX)};

//             auto transfer = rhi->GetCommandList(CommandQueueType::TRANSFER);

//             transfer->BeginRecording();

//             // cpu -> stage
//             transfer->UpdateBuffer(buffers[i].get(), &data,
//                                    sizeof(data)); // TODO: multithread

//             transfer->EndRecording();
//             // stage -> gpu
//             cmdlists[i] = transfer;
//         }));
//     }

//     for (auto &res : results) {
//         res.wait();
//     }
//     LOG_INFO("all task done");

//     QueueSubmitInfo submit_info{};
//     submit_info.queue_type = CommandQueueType::TRANSFER;
//     submit_info.command_lists.swap(cmdlists);
//     rhi->SubmitCommandLists(submit_info);

//     rhi->WaitGpuExecution(CommandQueueType::TRANSFER);
// }
// } // namespace TEST