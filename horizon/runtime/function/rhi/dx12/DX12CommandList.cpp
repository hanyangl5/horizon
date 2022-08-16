//#include <algorithm>
//
//#include <runtime/function/rhi/dx12/DX12Buffer.h>
//#include <runtime/function/rhi/dx12/DX12CommandList.h>
//
//namespace Horizon::RHI {
//
//DX12CommandList::DX12CommandList(
//    CommandQueueType type, ID3D12GraphicsCommandList6 *command_list) noexcept
//    : CommandList(type), m_command_list(command_list) {}
//
//DX12CommandList::~DX12CommandList() noexcept {}
//
//void DX12CommandList::BeginRecording() noexcept {
//    is_recoring = true;
//
//    // m_command_list->Reset();
//}
//
//void DX12CommandList::EndRecording() noexcept {
//    is_recoring = false;
//    m_command_list->Close();
//}
//
//// graphics commands
//void DX12CommandList::BeginRenderPass() noexcept {
//
//    if (!is_recoring) {
//        LOG_ERROR("command buffer isn't recording");
//        return;
//    }
//
//    if (m_type != CommandQueueType::GRAPHICS) {
//        LOG_ERROR("invalid commands for current commandlist, expect graphics "
//                  "commandlist");
//        return;
//    }
//
//    D3D12_CPU_DESCRIPTOR_HANDLE rtvCPUDescriptorHandle{};
//    D3D12_CPU_DESCRIPTOR_HANDLE dsvCPUDescriptorHandle{};
//
//    const float clearColor4[]{0.f, 0.f, 0.f, 0.f};
//    CD3DX12_CLEAR_VALUE clearValue{DXGI_FORMAT_R32G32B32_FLOAT, clearColor4};
//
//    D3D12_RENDER_PASS_BEGINNING_ACCESS renderPassBeginningAccessClear{
//        D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR, {clearValue}};
//    D3D12_RENDER_PASS_ENDING_ACCESS renderPassEndingAccessPreserve{
//        D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE, {}};
//    D3D12_RENDER_PASS_RENDER_TARGET_DESC renderPassRenderTargetDesc{
//        rtvCPUDescriptorHandle, renderPassBeginningAccessClear,
//        renderPassEndingAccessPreserve};
//
//    D3D12_RENDER_PASS_BEGINNING_ACCESS renderPassBeginningAccessNoAccess{
//        D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS, {}};
//    D3D12_RENDER_PASS_ENDING_ACCESS renderPassEndingAccessNoAccess{
//        D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS, {}};
//    D3D12_RENDER_PASS_DEPTH_STENCIL_DESC renderPassDepthStencilDesc{
//        dsvCPUDescriptorHandle, renderPassBeginningAccessNoAccess,
//        renderPassBeginningAccessNoAccess, renderPassEndingAccessNoAccess,
//        renderPassEndingAccessNoAccess};
//
//    m_command_list->BeginRenderPass(1, &renderPassRenderTargetDesc,
//                                    &renderPassDepthStencilDesc,
//                                    D3D12_RENDER_PASS_FLAG_NONE);
//}
//
//void DX12CommandList::EndRenderPass() noexcept {
//    if (!is_recoring) {
//        LOG_ERROR("command buffer isn't recording");
//        return;
//    }
//    if (m_type != CommandQueueType::GRAPHICS) {
//        LOG_ERROR("invalid commands for current commandlist, expect graphics "
//                  "commandlist");
//        return;
//    }
//
//    m_command_list->EndRenderPass();
//}
//void DX12CommandList::Draw() noexcept {
//    if (!is_recoring) {
//        LOG_ERROR("command buffer isn't recording");
//        return;
//    }
//    if (m_type != CommandQueueType::GRAPHICS) {
//        LOG_ERROR("invalid commands for current commandlist, expect graphics "
//                  "commandlist");
//        return;
//    }
//}
//
//void DX12CommandList::DrawIndirect() noexcept {
//    if (!is_recoring) {
//        LOG_ERROR("command buffer isn't recording");
//        return;
//    }
//    if (m_type != CommandQueueType::GRAPHICS) {
//        LOG_ERROR("invalid commands for current commandlist, expect graphics "
//                  "commandlist");
//        return;
//    }
//}
//
//// compute commands
//void DX12CommandList::Dispatch(u32 group_count_x, u32 group_count_y,
//                               u32 group_count_z) noexcept {
//    if (!is_recoring) {
//        LOG_ERROR("command buffer isn't recording");
//        return;
//    }
//    if (m_type != CommandQueueType::COMPUTE) {
//        LOG_ERROR("invalid commands for current commandlist, expect compute "
//                  "commandlist");
//        return;
//    }
//
//    // m_command_list->Dispatch(group_count_x, group_count_y, group_count_z);
//}
//void DX12CommandList::DispatchIndirect() noexcept {
//    if (!is_recoring) {
//        LOG_ERROR("command buffer isn't recording");
//        return;
//    }
//    if (m_type != CommandQueueType::COMPUTE) {
//        LOG_ERROR("invalid commands for current commandlist, expect compute "
//                  "commandlist");
//        return;
//    }
//}
//
//// transfer commands
//void DX12CommandList::UpdateBuffer(Buffer *buffer, void *data,
//                                   u64 size) noexcept {
//    if (!is_recoring) {
//        LOG_ERROR("command buffer isn't recording");
//        return;
//    }
//    if (m_type != CommandQueueType::TRANSFER) {
//        LOG_ERROR("invalid commands for current commandlist, expect transfer "
//                  "commandlist");
//        return;
//    }
//
//    assert(buffer->m_size == size);
//
//    // cannot update static buffer more than once
//
//    // frequently updated buffer
//    {
//        auto dx12_buffer = static_cast<DX12Buffer *>(buffer);
//
//        DX12Buffer *stage_buffer = GetStageBuffer(
//            dx12_buffer->m_allocator,
//            BufferCreateInfo{DescriptorType::DESCRIPTOR_TYPE_UNDEFINED,
//                             ResourceState::RESOURCE_STATE_COPY_SOURCE, size});
//
//        const auto &resource = stage_buffer->m_allocation->GetResource();
//
//        if (mapped_data && memcmp(mapped_data, data, size) == 0) {
//            // LOG_DEBUG("prev buffer and current buffer are same");
//            return;
//        }
//
//        CHECK_DX_RESULT(resource->Map(0, nullptr, &mapped_data));
//        memcpy(mapped_data, data, size);
//        resource->Unmap(0, nullptr);
//
//        // barrier 1
//        {
//            BufferBarrierDesc bmb{};
//            bmb.buffer = stage_buffer;
//
//            BarrierDesc desc{};
//
//            desc.buffer_memory_barriers.emplace_back(bmb);
//
//            InsertBarrier(desc);
//        }
//
//        // copy to gpu buffer
//        CopyBuffer(stage_buffer, dx12_buffer);
//    }
//}
//
//void DX12CommandList::CopyBuffer(Buffer *src_buffer,
//                                 Buffer *dst_buffer) noexcept {
//    if (!is_recoring) {
//        LOG_ERROR("command buffer isn't recording");
//        return;
//    }
//    if (m_type != CommandQueueType::TRANSFER) {
//        LOG_ERROR("invalid commands for current commandlist, expect transfer "
//                  "commandlist");
//        return;
//    }
//    auto dx12_src_buffer = static_cast<DX12Buffer *>(src_buffer);
//    auto dx12_dst_buffer = static_cast<DX12Buffer *>(dst_buffer);
//    CopyBuffer(dx12_src_buffer, dx12_dst_buffer);
//}
//
//void DX12CommandList::CopyBuffer(DX12Buffer *src_buffer,
//                                 DX12Buffer *dst_buffer) noexcept {
//    assert(dst_buffer->m_size == src_buffer->m_size);
//
//    // copy buffer
//    m_command_list->CopyResource(dst_buffer->m_allocation->GetResource(),
//                                 src_buffer->m_allocation->GetResource());
//}
//
//void DX12CommandList::UpdateTexture(Texture *texture,
//                                    const TextureData &texture_data) noexcept {
//    if (!is_recoring) {
//        LOG_ERROR("command buffer isn't recording");
//        return;
//    }
//    if (m_type != CommandQueueType::TRANSFER) {
//        LOG_ERROR("invalid commands for current commandlist, expect transfer "
//                  "commandlist");
//    }
//}
//
//void DX12CommandList::CopyTexture() noexcept {
//    if (!is_recoring) {
//        LOG_ERROR("command buffer isn't recording");
//        return;
//    }
//    if (m_type != CommandQueueType::TRANSFER) {
//        LOG_ERROR("invalid commands for current commandlist, expect transfer "
//                  "commandlist");
//    }
//}
//
//void DX12CommandList::InsertBarrier(const BarrierDesc &desc) noexcept {
//    if (!is_recoring) {
//        LOG_ERROR("command buffer isn't recording");
//        return;
//    }
//
//    std::vector<D3D12_RESOURCE_BARRIER> barriers{};
//
//    m_command_list->ResourceBarrier(barriers.size(), barriers.data());
//}
//
//void DX12CommandList::BindPipeline(Pipeline *pipeline) noexcept {}
//
//DX12Buffer *DX12CommandList::GetStageBuffer(
//    D3D12MA::Allocator *allocator,
//    const BufferCreateInfo &buffer_create_info) noexcept {
//    if (m_stage_buffer) {
//        return m_stage_buffer.get();
//    } else {
//        m_stage_buffer = std::make_unique<DX12Buffer>(
//            allocator, buffer_create_info, MemoryFlag::CPU_VISABLE_MEMORY);
//        return m_stage_buffer.get();
//    }
//}
//
//} // namespace Horizon::RHI
