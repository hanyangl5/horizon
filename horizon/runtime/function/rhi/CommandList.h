#pragma once

#include <runtime/function/resource/IndexBuffer.h>
#include <runtime/function/resource/VertexBuffer.h>
#include <runtime/function/rhi/Buffer.h>
#include <runtime/function/rhi/Pipeline.h>
#include <runtime/function/rhi/RHIUtils.h>
#include <runtime/function/rhi/ResourceBarrier.h>
#include <runtime/function/rhi/Texture.h>

namespace Horizon::RHI {

class CommandList {
  public:
    CommandList(CommandQueueType type) noexcept;
    virtual ~CommandList() noexcept;

    virtual void BeginRecording() noexcept = 0;
    virtual void EndRecording() noexcept = 0;

    virtual void BindVertexBuffer(u32 buffer_count, Buffer **buffers, u32 *offsets) noexcept = 0;
    virtual void BindIndexBuffer(Buffer *buffer, u32 offset) noexcept = 0;

    virtual void BeginRenderPass(const RenderPassBeginInfo &begin_info) noexcept = 0;
    virtual void EndRenderPass() noexcept = 0;
    virtual void DrawInstanced(u32 vertex_count, u32 first_vertex, u32 instance_count = 1,
                               u32 first_instance = 0) noexcept = 0;
    virtual void DrawIndexedInstanced(u32 index_count, u32 first_index, u32 first_vertex, u32 instance_count = 1,
                                      u32 first_instance = 0) noexcept = 0;
    virtual void DrawIndirect() noexcept = 0;

    virtual void Dispatch(u32 group_count_x, u32 group_count_y, u32 group_count_z) noexcept = 0;
    virtual void DispatchIndirect() noexcept = 0;

    virtual void UpdateBuffer(Buffer *buffer, void *data, u64 size) noexcept = 0;
    virtual void UpdateTexture(Texture *texture, const TextureData &texture_data) noexcept = 0;
    virtual void CopyBuffer(Buffer *dst_buffer, Buffer *src_buffer) noexcept = 0;
    virtual void CopyTexture() noexcept = 0;

    virtual void InsertBarrier(const BarrierDesc &desc) noexcept = 0;

    virtual void BindPipeline(Pipeline *pipeline) noexcept = 0;

  protected:
    bool is_recoring{false};
    CommandQueueType m_type{};
};
} // namespace Horizon::RHI