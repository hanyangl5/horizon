#pragma once

#include <vulkan/vulkan.h>

#include <runtime/rhi/CommandList.h>
#include <runtime/rhi/vulkan/VulkanBuffer.h>
#include <runtime/rhi/vulkan/VulkanTexture.h>
#include <runtime/rhi/vulkan/VulkanUtils.h>

namespace Horizon::Backend {

class VulkanCommandList : public CommandList {
  public:
    VulkanCommandList(const VulkanRendererContext &context, CommandQueueType type,
                      VkCommandBuffer command_buffer) noexcept;

    virtual ~VulkanCommandList() noexcept;
    VulkanCommandList(const VulkanCommandList &rhs) noexcept = delete;
    VulkanCommandList &operator=(const VulkanCommandList &rhs) noexcept = delete;
    VulkanCommandList(VulkanCommandList &&rhs) noexcept = delete;
    VulkanCommandList &operator=(VulkanCommandList &&rhs) noexcept = delete;

    virtual void BeginRecording() override;
    virtual void EndRecording() override;

    virtual void BindVertexBuffers(u32 buffer_count, Buffer **buffers, u32 *offsets) override;
    virtual void BindIndexBuffer(Buffer *buffer, u32 offset) override;

    // graphics commands
    void BeginRenderPass(const RenderPassBeginInfo &begin_info) override;
    void EndRenderPass() override;

    void DrawInstanced(u32 vertex_count, u32 first_vertex, u32 instance_count = 1, u32 first_instance = 0) override;

    virtual void DrawIndexedInstanced(u32 index_count, u32 first_index, u32 first_vertex, u32 instance_count = 1,
                                      u32 first_instance = 0) override;
    void DrawIndirect() override;
    void DrawIndirectIndexedInstanced(Buffer *buffer, u32 offset, u32 draw_count, u32 stride) override;
    // compute commands
    virtual void Dispatch(u32 group_count_x, u32 group_count_y, u32 group_count_z) override;
    virtual void DispatchIndirect() override;

    void UpdateBuffer(Buffer *buffer, void *data, u64 size) override;

    void CopyBuffer(Buffer *src_buffer, Buffer *dst_buffer) override;

    void CopyTexture(Texture *src_texture, Texture *dst_texture) override;

    void CopyBuffer(VulkanBuffer *src_buffer, VulkanBuffer *dst_buffer);

    void CopyTexture(VulkanTexture *src_texture, VulkanTexture *dst_texture);

    //void CopyBufferToTexture(VulkanBuffer *src_buffer, VulkanTexture *dst_texture) override;
    //
    // void CopyTextureToBuffer(VulkanTexture *src_texture, VulkanBuffer *dst_buffer) override;

    void UpdateTexture(Texture *texture, const TextureUpdateDesc &texture_data) override;

    virtual void InsertBarrier(const BarrierDesc &desc) override;

    virtual void BindPipeline(Pipeline *pipeline) override;

    void BindPushConstant(Pipeline *pipeline, const std::string &name, void *data) override;

    void BindPushConstant(Pipeline *pipeline, u32 index, void *data) override;

    void ClearBuffer(Buffer *buffer, f32 clear_value) override;

    void ClearTextrue(Texture *texture, const Math::float4 &clear_value) override;

    void BindDescriptorSets(Pipeline *pipeline, DescriptorSet *set) override;

    void GenerateMipMap(Texture *texture, bool alllevels) override;
    void BeginQuery() override {
        //vkCmdBeginQuery(m_command_buffer, m_context.gpu_query_pool, 0, VK_QUERY_CONTROL_PRECISE_BIT);
        vkCmdWriteTimestamp(m_command_buffer,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT , m_context.gpu_query_pool, 0);
    }
    void EndQuery() override {
        //vkCmdEndQuery(m_command_buffer, m_context.gpu_query_pool, 0);
        vkCmdWriteTimestamp(m_command_buffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, m_context.gpu_query_pool, 1);
    }

  private:
    const VulkanRendererContext &m_context{};
    VulkanBuffer *GetStageBuffer(const BufferCreateInfo &buffer_create_info);
    public : VkCommandBuffer m_command_buffer{};
};

} // namespace Horizon::Backend
