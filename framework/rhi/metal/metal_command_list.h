#pragma once

#include <rhi/command_list.h>
#include <rhi/metal/metal_utils.h>

namespace Horizon::Backend
{

class MetalBuffer;
class MetalPipeline;
class MetalSwapChain;

class MetalCommandList final : public CommandList
{
  public:
    explicit MetalCommandList(MTL::CommandQueue *command_queue) noexcept;
    ~MetalCommandList() noexcept override;
    MetalCommandList(const MetalCommandList &rhs) noexcept = delete;
    MetalCommandList &operator=(const MetalCommandList &rhs) noexcept = delete;
    MetalCommandList(MetalCommandList &&rhs) noexcept = delete;
    MetalCommandList &operator=(MetalCommandList &&rhs) noexcept = delete;

    void BeginRecording() override;
    void EndRecording() override;

    void BindVertexBuffers(u32 buffer_count, Buffer **buffers, u32 *offsets) override;
    void BindIndexBuffer(Buffer *buffer, u32 offset) override;

    void BeginRenderPass(const RenderPassBeginInfo &begin_info) override;
    void EndRenderPass() override;
    void BeginComputePass(const char *debug_name = nullptr) override;
    void EndComputePass() override;

    void DrawInstanced(u32 vertex_count, u32 first_vertex, u32 instance_count = 1, u32 first_instance = 0) override;
    void DrawIndexedInstanced(u32 index_count, u32 first_index, u32 first_vertex, u32 instance_count = 1,
                              u32 first_instance = 0) override;
    void DrawIndirect() override;
    void DrawIndirectIndexedInstanced(Buffer *buffer, u32 offset, u32 draw_count, u32 stride) override;
    void DrawMeshTasks(u32 group_count_x, u32 group_count_y = 1, u32 group_count_z = 1) override;
    void Dispatch(u32 group_count_x, u32 group_count_y, u32 group_count_z) override;
    void DispatchIndirect() override;

    void UpdateBuffer(Buffer *buffer, void *data, u64 size) override;
    void UpdateTexture(Texture *texture, const TextureUpdateDesc &texture_data) override;
    void CopyBuffer(Buffer *dst_buffer, Buffer *src_buffer) override;
    void CopyTexture(Texture *src_texture, Texture *dst_texture) override;
    void InsertBarrier(const BarrierDesc &desc) override;

    void BindPipeline(Pipeline *pipeline) override;
    void BindPushConstant(Pipeline *pipeline, const std::string &name, void *data) override;

    void ClearBuffer(Buffer *buffer, f32 clear_value) override;
    void ClearTextrue(Texture *texture, const ClearColorValue &clear_value) override;
    void GenerateMipMap(Texture *texture) override;

    void BeginQuery() override;
    void EndQuery() override;

    void SetActiveSwapChain(MetalSwapChain *swap_chain) noexcept;

  private:
    friend class MetalRHI;

    MTL::CommandQueue *m_command_queue{};
    MTL::CommandBuffer *m_command_buffer{};
    MTL::RenderCommandEncoder *m_render_encoder{};
    MetalPipeline *m_bound_pipeline{};
    MetalBuffer *m_index_buffer{};
    u32 m_index_buffer_offset{};
    MetalSwapChain *m_present_swap_chain{};
    MetalSwapChain *m_active_swap_chain{};
};

} // namespace Horizon::Backend
