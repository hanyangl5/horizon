//#include "DX12CommandContext.h"
//#include "DX12Buffer.h"
//
//namespace Horizon::RHI {
//
//DX12CommandContext::DX12CommandContext(ID3D12Device *device) noexcept
//    : m_device(device) {
//    m_command_lists_count.fill(0);
//}
//
//DX12CommandContext::~DX12CommandContext() noexcept {
//    // TODO: destroy command allocator
//    Reset();
//}
//
//CommandList *DX12CommandContext::GetCommandList(CommandQueueType type) noexcept {
//    // lazy create command pool
//    if (m_command_pools[type] == nullptr) {
//
//        m_device->CreateCommandAllocator(ToDX12CommandQueueType(type),
//                                         IID_PPV_ARGS(&m_command_pools[type]));
//    }
//
//    u32 count{m_command_lists_count[type]};
//
//    if (count >= m_command_lists[type].size()) {
//
//        ID3D12GraphicsCommandList6 *command_list{};
//        m_device->CreateCommandList(0, ToDX12CommandQueueType(type),
//                                    m_command_pools[type], nullptr,
//                                    IID_PPV_ARGS(&command_list));
//
//        m_command_lists[type].emplace_back(
//            new DX12CommandList(type, command_list));
//    }
//
//    m_command_lists_count[type]++;
//    return m_command_lists[type][count];
//}
//
//void DX12CommandContext::Reset() noexcept {
//    // reset command buffers to initial state
//
//    for (auto &command_pool : m_command_pools) {
//        if (command_pool) {
//            command_pool->Reset();
//        }
//    }
//
//    // for (u32 type = 0; type < 3;type++) {
//    //	for (auto& cmdlist : m_command_lists[type]) {
//    //		if (cmdlist) {
//    //			//vkFreeCommandBuffers(m_device, m_command_pools[type],
//    //1, &cmdlist->m_command
//    //  reset command buffers to reuse_buffer);
//    //			//vkResetCommandBuffer(cmdlist->m_command_buffer,
//    // VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
//    //		}
//    //	}
//    // }
//
//    m_command_lists_count.fill(0);
//}
//} // namespace Horizon::RHI
