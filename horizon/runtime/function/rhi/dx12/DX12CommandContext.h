#pragma once

#include <runtime/function/rhi/CommandList.h>
#include "DX12Buffer.h"

namespace Horizon
{
    namespace RHI
    {

        //class DX12CommandList : public CommandContext
        //{
        //public:
        //    DX12CommandList() noexcept;
        //    ~DX12CommandList() noexcept;
        //    virtual void BeginRecording() noexcept override;
        //    virtual void EndRecording() noexcept override;

        //    virtual void Draw() noexcept override;
        //    virtual void DrawIndirect() noexcept override;
        //    virtual void Dispatch() noexcept override;
        //    virtual void DispatchIndirect() noexcept override;
        //    virtual void UpdateBuffer(Buffer* buffer, void* data, u64 size) noexcept override;
        //    virtual void UpdateTexture() noexcept override;

        //    virtual void CopyBuffer(Buffer* src_buffer, Buffer* dst_buffer) noexcept override;
        //    virtual void CopyTexture() noexcept override;

        //    virtual void InsertBarrier(const BarrierDesc& desc) noexcept override;
        //    virtual void Submit() noexcept override;

        //private:
        //    void CopyBuffer(DX12Buffer* src_buffer, DX12Buffer* dst_buffer) noexcept;
        //private:
        //    //VkCommandBuffer m_command_buffer;

        //    // DX12CommandListType m_type;
        //};
    }
}
