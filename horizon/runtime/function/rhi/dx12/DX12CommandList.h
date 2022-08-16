//#pragma once
//
//#include "stdafx.h"
//
//#include <array>
//
//#include <runtime/function/rhi/CommandList.h>
//#include <runtime/function/rhi/dx12/DX12Buffer.h>
//#include <runtime/function/rhi/dx12/DX12Texture.h>
//
//#include <D3D12MemAlloc.h>
//
//namespace Horizon::RHI {
//
//class DX12CommandList : public CommandList {
//  public:
//    DX12CommandList(CommandQueueType type,
//                    ID3D12GraphicsCommandList6 *command_list) noexcept;
//    virtual ~DX12CommandList() noexcept;
//
//    void BeginRecording() noexcept override;
//    void EndRecording() noexcept override;
//
//    // graphics commands
//    void BeginRenderPass() noexcept override;
//    void EndRenderPass() noexcept override;
//    void Draw() noexcept override;
//    void DrawIndirect() noexcept override;
//
//    // compute commands
//    void Dispatch(u32 group_count_x, u32 group_count_y,
//                  u32 group_count_z) noexcept override;
//    void DispatchIndirect() noexcept override;
//
//    void UpdateBuffer(Buffer *buffer, void *data, u64 size) noexcept override;
//    void CopyBuffer(Buffer *src_buffer, Buffer *dst_buffer) noexcept override;
//    void CopyBuffer(DX12Buffer *src_buffer, DX12Buffer *dst_buffer) noexcept;
//    void UpdateTexture(Texture *texture,
//                       const TextureData &texture_data) noexcept override;
//
//    void CopyTexture() noexcept override;
//
//    void InsertBarrier(const BarrierDesc &desc) noexcept override;
//
//    void BindPipeline(Pipeline *pipeline) noexcept override;
//
//  private:
//    DX12Buffer *
//    GetStageBuffer(D3D12MA::Allocator *allocator,
//                   const BufferCreateInfo &buffer_create_info) noexcept;
//
//  public:
//    ID3D12GraphicsCommandList6 *m_command_list{};
//    Resource<DX12Buffer> m_stage_buffer{};
//    void *mapped_data{};
//};
//
//} // namespace Horizon::RHI
