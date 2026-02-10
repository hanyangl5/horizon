#pragma once

#include "dx12_command_allocator.h"
#include "dx12_utils.h"
#include <d3d12.h>
#include <wrl/client.h>

#include <core/definations.h>
#include <rhi/command_list.h>
#include <rhi/pipeline.h>

namespace Horizon::Backend
{

class DX12CommandList : public CommandList
{
  public:
    DX12CommandList(const DX12RendererContext &context, CommandQueueType type,
                    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> command_list,
                    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator) noexcept;

    virtual ~DX12CommandList() noexcept;
    DX12CommandList(const DX12CommandList &rhs) noexcept = delete;
    DX12CommandList &operator=(const DX12CommandList &rhs) noexcept = delete;
    DX12CommandList(DX12CommandList &&rhs) noexcept = delete;
    DX12CommandList &operator=(DX12CommandList &&rhs) noexcept = delete;

    virtual void BeginRecording() override;
    virtual void EndRecording() override;

    virtual void BindVertexBuffers(u32 buffer_count, Buffer **buffers, u32 *offsets) override;
    virtual void BindIndexBuffer(Buffer *buffer, u32 offset) override;

    void BeginRenderPass(const RenderPassBeginInfo &begin_info) override;
    void EndRenderPass() override;
    void BeginComputePass(const char *debug_name = nullptr) override;
    void EndComputePass() override;

    void DrawInstanced(u32 vertex_count, u32 first_vertex, u32 instance_count = 1, u32 first_instance = 0) override;
    virtual void DrawIndexedInstanced(u32 index_count, u32 first_index, u32 first_vertex, u32 instance_count = 1,
                                      u32 first_instance = 0) override;
    void DrawIndirect() override;
    void DrawIndirectIndexedInstanced(Buffer *buffer, u32 offset, u32 draw_count, u32 stride) override;

    virtual void Dispatch(u32 group_count_x, u32 group_count_y, u32 group_count_z) override;
    virtual void DispatchIndirect() override;

    void UpdateBuffer(Buffer *buffer, void *data, u64 size) override;
    void CopyBuffer(Buffer *src_buffer, Buffer *dst_buffer) override;
    void CopyTexture(Texture *src_texture, Texture *dst_texture) override;
    void UpdateTexture(Texture *texture, const TextureUpdateDesc &texture_data) override;

    virtual void InsertBarrier(const BarrierDesc &desc) override;

    virtual void BindPipeline(Pipeline *pipeline) override;
    void BindPushConstant(Pipeline *pipeline, const std::string &name, void *data) override;

    void ClearBuffer(Buffer *buffer, f32 clear_value) override;
    void ClearTextrue(Texture *texture, const ClearColorValue &clear_value) override;

    void GenerateMipMap(Texture *texture) override;
    void BeginQuery() override;
    void EndQuery() override;

    ID3D12GraphicsCommandList *GetD3D12CommandList() const noexcept
    {
        return m_command_list.Get();
    }

  private:
    const DX12RendererContext &m_context;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_command_list;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_allocator;
    bool m_is_recording{false};
    bool m_in_render_pass{false};
    bool m_in_compute_pass{false};
    Pipeline *m_current_pipeline{nullptr}; // Track current pipeline for vertex stride
};

} // namespace Horizon::Backend
