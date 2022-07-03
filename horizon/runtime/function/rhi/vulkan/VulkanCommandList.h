#pragma once

#include <vulkan/vulkan.h>

#include <array>

#include <runtime/function/rhi/CommandList.h>
#include <runtime/function/rhi/vulkan/VulkanBuffer.h>

namespace Horizon::RHI {

class VulkanCommandList : public CommandList {
  public:
    VulkanCommandList(CommandQueueType type,
                      VkCommandBuffer command_buffer) noexcept;
    virtual ~VulkanCommandList() noexcept;

    virtual void BeginRecording() noexcept override;
    virtual void EndRecording() noexcept override;

    // graphics commands
    virtual void BeginRenderPass() noexcept override;
    virtual void EndRenderPass() noexcept override;
    virtual void Draw() noexcept override;
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

    virtual void UpdateTexture() noexcept override;

    virtual void CopyTexture() noexcept override;

    virtual void InsertBarrier(const BarrierDesc &desc) noexcept override;

    virtual void BindPipeline(Pipeline *pipeline) noexcept override;

  private:
    VulkanBuffer *
    GetStageBuffer(VmaAllocator allocator,
                   const BufferCreateInfo &buffer_create_info) noexcept;

  public:
    VkCommandBuffer m_command_buffer;       // TODO: release command buffer
    VulkanBuffer *m_stage_buffer = nullptr; // TODO: release stage buffer
};

} // namespace Horizon::RHI
