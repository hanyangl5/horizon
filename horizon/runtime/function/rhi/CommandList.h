#pragma once

#include <variant>

#include <runtime/core/math/Math.h>
#include <runtime/function/rhi/Buffer.h>
#include <runtime/function/rhi/Pipeline.h>
#include <runtime/function/rhi/RHIUtils.h>
#include <runtime/function/rhi/RenderTarget.h>
#include <runtime/function/rhi/ResourceBarrier.h>
#include <runtime/function/rhi/Texture.h>

namespace Horizon::Backend {

// TODO: move these definations to RHIUtils.h

struct RenderTargetInfo {
    RenderTarget *data{};
    std::variant<ClearColorValue, ClearValueDepthStencil> clear_color{};
};

struct RenderPassBeginInfo {
    std::array<RenderTargetInfo, MAX_RENDER_TARGET_COUNT> render_targets{}; // TODO(hylu): FixedArray
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
    virtual void DrawIndirectIndexedInstanced(Buffer* buffer, u32 offset, u32 draw_count, u32 stride) = 0;
    virtual void Dispatch(u32 group_count_x, u32 group_count_y, u32 group_count_z) = 0;
    virtual void DispatchIndirect() = 0;

    virtual void UpdateBuffer(Buffer *buffer, void *data, u64 size) = 0;
    virtual void UpdateTexture(Texture *texture, const TextureUpdateDesc &texture_data) = 0;
    virtual void CopyBuffer(Buffer *dst_buffer, Buffer *src_buffer) = 0;
    virtual void CopyTexture(Texture *src_texture, Texture *dst_texture) = 0;
    // virtual void CopyBufferToTexture(VulkanBuffer *src_buffer, VulkanTexture *dst_texture) = 0;
    
    virtual void InsertBarrier(const BarrierDesc &desc) = 0;

    virtual void BindPipeline(Pipeline *pipeline) = 0;

    virtual void BindPushConstant(Pipeline *pipeline, const Container::String &name, void *data) = 0;

    virtual void ClearBuffer(Buffer *buffer, f32 clear_value) = 0;

    virtual void ClearTextrue(Texture *texture, const Math::float4 &clear_value) = 0;

    // bind by index save string lookup
    virtual void BindPushConstant(Pipeline *pipeline, u32 index, void *data) = 0;

    virtual void BindDescriptorSets(Pipeline *pipeline, DescriptorSet *set) = 0;

    virtual void GenerateMipMap(Texture *texture, bool alllevels = true) = 0;

  protected:
    bool is_recoring{false};
    CommandQueueType m_type{};
};
} // namespace Horizon::Backend