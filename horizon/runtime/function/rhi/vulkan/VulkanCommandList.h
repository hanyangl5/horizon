#pragma once

#include <vulkan/vulkan.h>

#include <array>

#include <runtime/function/rhi/CommandList.h>
#include <runtime/function/rhi/vulkan/VulkanBuffer.h>
#include <runtime/function/rhi/vulkan/VulkanTexture.h>
#include <runtime/function/rhi/vulkan/VulkanUtils.h>

namespace Horizon::RHI {

class VulkanCommandList : public CommandList {
  public:
    VulkanCommandList(const VulkanRendererContext &context,
                      CommandQueueType type,
                      VkCommandBuffer command_buffer) noexcept;

    virtual ~VulkanCommandList() noexcept;

    virtual void BeginRecording() noexcept override;
    virtual void EndRecording() noexcept override;

    virtual void BindVertexBuffer(u32 buffer_count, VertexBuffer **buffers,
                                  u32 *offsets) noexcept override;
    virtual void BindIndexBuffer(IndexBuffer *buffer,
                                 u32 offset) noexcept override;

    // graphics commands
    virtual void
    BeginRenderPass(const RenderPassBeginInfo &begin_info) noexcept override;
    virtual void EndRenderPass() noexcept override;

    virtual void DrawInstanced(u32 vertex_count, u32 first_vertex,
                               u32 instance_count = 1,
                               u32 first_instance = 0) noexcept override;

    virtual void DrawIndexedInstanced(u32 index_count, u32 first_index,
                                      u32 first_vertex, u32 instance_count = 1,
                                      u32 first_instance = 0) noexcept override;
    virtual void DrawIndirect() noexcept override;
    // compute commands
    virtual void Dispatch(u32 group_count_x, u32 group_count_y,
                          u32 group_count_z) noexcept override;
    virtual void DispatchIndirect() noexcept override;

    virtual void UpdateBuffer(Buffer *buffer, void *data,
                              u64 size) noexcept override;
    virtual void CopyBuffer(Buffer *src_buffer,
                            Buffer *dst_buffer) noexcept override;
    void CopyBuffer(VulkanBuffer *src_buffer,
                    VulkanBuffer *dst_buffer) noexcept;

    void UpdateTexture(Texture *texture,
                       const TextureData &texture_data) noexcept override;

    virtual void CopyTexture() noexcept override;

    virtual void InsertBarrier(const BarrierDesc &desc) noexcept override;

    virtual void BindPipeline(Pipeline *pipeline) noexcept override;

    void CopyBufferToImage() noexcept;

  private:
    const VulkanRendererContext &m_context;
    Resource<VulkanBuffer>
    GetStageBuffer(VmaAllocator allocator,
                   const BufferCreateInfo &buffer_create_info) noexcept;

  public:
    VkCommandBuffer m_command_buffer;
};

} // namespace Horizon::RHI
