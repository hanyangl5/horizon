#pragma once

#include "stdafx.h"

#include <array>

#include <runtime/function/rhi/CommandList.h>
#include <runtime/function/rhi/dx12/DX12Buffer.h>

#include <third_party/D3D12MemoryAllocator/include/D3D12MemAlloc.h>

namespace Horizon
{
    namespace RHI
    {

        class DX12CommandList : public CommandList{
        public:
            DX12CommandList(CommandQueueType type, ID3D12GraphicsCommandList6* command_list) noexcept;
            virtual ~DX12CommandList() noexcept;

            virtual void BeginRecording() noexcept override;
            virtual void EndRecording() noexcept override;

            // graphics commands
            virtual void BeginRenderPass() noexcept override;
            virtual void EndRenderPass() noexcept override;
            virtual void Draw() noexcept override;
            virtual void DrawIndirect() noexcept override;

            // compute commands
            virtual void Dispatch(u32 group_count_x, u32 group_count_y, u32 group_count_z) noexcept override;
            virtual void DispatchIndirect() noexcept override;

            virtual void UpdateBuffer(Buffer* buffer, void* data, u64 size) noexcept override;
			virtual void CopyBuffer(Buffer* src_buffer, Buffer* dst_buffer) noexcept override;
			void CopyBuffer(DX12Buffer* src_buffer, DX12Buffer* dst_buffer) noexcept;

            virtual void UpdateTexture() noexcept override;

            virtual void CopyTexture() noexcept override;

            virtual void InsertBarrier(const BarrierDesc& desc) noexcept override;

            void BindPipeline(Pipeline& pipeline) noexcept override;
        private:
            DX12Buffer* GetStageBuffer(D3D12MA::Allocator* allocator, const BufferCreateInfo& buffer_create_info) noexcept;
        public:
            ID3D12GraphicsCommandList6* m_command_list = nullptr;
            DX12Buffer* m_stage_buffer = nullptr;
            void* mapped_data = nullptr;
        };

    }
}
