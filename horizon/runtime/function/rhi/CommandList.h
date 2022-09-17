#pragma once

#include <runtime/core/math/Math.h>
#include <runtime/function/rhi/Buffer.h>
#include <runtime/function/rhi/Texture.h>
#include <runtime/function/rhi/RenderTarget.h>
#include <runtime/function/rhi/Pipeline.h>
#include <runtime/function/rhi/RHIUtils.h>
#include <runtime/function/rhi/ResourceBarrier.h>
#include <runtime/function/resource/IndexBuffer.h>
#include <runtime/function/resource/VertexBuffer.h>

namespace Horizon::RHI {

struct RenderTargetInfo{
    RenderTarget* data{};
    Math::float4 clear_color{};
};

struct RenderPassBeginInfo {
    std::array<RenderTargetInfo, MAX_RENDER_TARGET_COUNT> render_targets;
    RenderTargetInfo depth, stencil;
    Rect render_area;
};

class CommandList {
  public:
    CommandList(CommandQueueType type) noexcept;
    virtual ~CommandList() noexcept;
    CommandList(const CommandList &rhs) noexcept = delete;
    CommandList &operator=(const CommandList &rhs) noexcept = delete;
    CommandList(CommandList &&rhs) noexcept = delete;
    CommandList &operator=(CommandList &&rhs) noexcept = delete;

    virtual void BeginRecording() = 0;
    virtual void EndRecording() = 0;

    virtual void BindVertexBuffers(u32 buffer_count, Buffer **buffers, u32 *offsets) = 0;
    virtual void BindIndexBuffer(Buffer *buffer, u32 offset) = 0;

    virtual void BeginRenderPass(const RenderPassBeginInfo &begin_info) = 0;
    virtual void EndRenderPass() = 0;
    virtual void DrawInstanced(u32 vertex_count, u32 first_vertex, u32 instance_count = 1, u32 first_instance = 0) = 0;
    virtual void DrawIndexedInstanced(u32 index_count, u32 first_index, u32 first_vertex, u32 instance_count = 1,
                                      u32 first_instance = 0) = 0;
    virtual void DrawIndirect() = 0;

    virtual void Dispatch(u32 group_count_x, u32 group_count_y, u32 group_count_z) = 0;
    virtual void DispatchIndirect() = 0;

    virtual void UpdateBuffer(Buffer *buffer, void *data, u64 size) = 0;
    virtual void UpdateTexture(Texture *texture, const TextureData &texture_data) = 0;
    virtual void CopyBuffer(Buffer *dst_buffer, Buffer *src_buffer) = 0;
    virtual void CopyTexture() = 0;

    virtual void InsertBarrier(const BarrierDesc &desc) = 0;

    virtual void BindPipeline(Pipeline *pipeline) = 0;
     
    virtual void BindPushConstant(Pipeline *pipeline, const std::string& name, void *data) = 0;

    // bind by index save string lookup
    virtual void BindPushConstant(Pipeline *pipeline, u32 index, void *data) = 0;
  protected:
    bool is_recoring{false};
    CommandQueueType m_type{};
};
} // namespace Horizon::RHI