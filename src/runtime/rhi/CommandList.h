#pragma once

#include <variant>

#include <runtime/core/math/Math.h>
#include <runtime/rhi/Buffer.h>
#include <runtime/rhi/Pipeline.h>
#include <runtime/rhi/Enums.h>
#include <runtime/rhi/RenderTarget.h>
#include <runtime/rhi/ResourceBarrier.h>
#include <runtime/rhi/Texture.h>

namespace Horizon::Backend {

// TODO: move these definations to Enums.h

struct RenderTargetInfo {
    RenderTarget *data{};
    std::variant<ClearColorValue, ClearValueDepthStencil> clear_color{};
    RenderTargetLoadOp load_op;
    RenderTargetStoreOp store_op;
};

struct RenderPassBeginInfo {
    u32 render_target_count{};
    std::array<RenderTargetInfo, MAX_RENDER_TARGET_COUNT> render_targets{};
    RenderTargetInfo depth_stencil{};
    Rect render_area{};
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
    virtual void DrawIndirectIndexedInstanced(Buffer *buffer, u32 offset, u32 draw_count, u32 stride) = 0;
    virtual void Dispatch(u32 group_count_x, u32 group_count_y, u32 group_count_z) = 0;
    virtual void DispatchIndirect() = 0;

    virtual void UpdateBuffer(Buffer *buffer, void *data, u64 size) = 0;
    virtual void UpdateTexture(Texture *texture, const TextureUpdateDesc &texture_data) = 0;
    virtual void CopyBuffer(Buffer *dst_buffer, Buffer *src_buffer) = 0;
    virtual void CopyTexture(Texture *src_texture, Texture *dst_texture) = 0;
    // virtual void CopyBufferToTexture(VulkanBuffer *src_buffer, VulkanTexture *dst_texture) = 0;

    virtual void InsertBarrier(const BarrierDesc &desc) = 0;

    virtual void BindPipeline(Pipeline *pipeline) = 0;

    virtual void BindPushConstant(Pipeline *pipeline, const std::string &name, void *data) = 0;

    virtual void ClearBuffer(Buffer *buffer, f32 clear_value) = 0;

    virtual void ClearTextrue(Texture *texture, const ClearColorValue &clear_value) = 0;

    virtual void BindDescriptorSets(Pipeline *pipeline, DescriptorSet *set) = 0;

    virtual void GenerateMipMap(Texture *texture) = 0;

    virtual void BeginQuery() = 0;
    virtual void EndQuery() = 0;

  protected:
    CommandQueueType m_type{};
};
} // namespace Horizon::Backend