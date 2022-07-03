#pragma once

#include <runtime/function/rhi/Buffer.h>
#include <runtime/function/rhi/Pipeline.h>
#include <runtime/function/rhi/RHIUtils.h>
#include <runtime/function/rhi/ResourceBarrier.h>

namespace Horizon::RHI {

class CommandList {
  public:
    CommandList(CommandQueueType type) noexcept;
    virtual ~CommandList() noexcept;

    virtual void BeginRecording() noexcept = 0;
    virtual void EndRecording() noexcept = 0;

    virtual void BeginRenderPass() noexcept = 0;
    virtual void EndRenderPass() noexcept = 0;
    virtual void Draw() noexcept = 0;
    virtual void DrawIndirect() noexcept = 0;

    virtual void Dispatch(u32 group_count_x, u32 group_count_y,
                          u32 group_count_z) noexcept = 0;
    virtual void DispatchIndirect() noexcept = 0;

    virtual void UpdateBuffer(Buffer *buffer, void *data,
                              u64 size) noexcept = 0;
    virtual void UpdateTexture() noexcept = 0;
    virtual void CopyBuffer(Buffer *dst_buffer,
                            Buffer *src_buffer) noexcept = 0;
    virtual void CopyTexture() noexcept = 0;

    virtual void InsertBarrier(const BarrierDesc &desc) noexcept = 0;

    virtual void BindPipeline(Pipeline *pipeline) noexcept = 0;

  protected:
    bool is_recoring{false};
    CommandQueueType m_type{};
};
} // namespace Horizon::RHI