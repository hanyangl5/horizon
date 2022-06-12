#pragma once

#include <runtime/function/rhi/CommandContext.h>
#include "VulkanBuffer2.h"

namespace Horizon
{
    namespace RHI
    {

        class VulkanCommandContext : public CommandContext
        {
        public:
            VulkanCommandContext(VkDevice device) noexcept;
            ~VulkanCommandContext() noexcept;
            virtual void BeginRecording() noexcept override;
            virtual void EndRecording() noexcept override;

            virtual void Draw() noexcept override;
            virtual void DrawIndirect() noexcept override;
            virtual void Dispatch() noexcept override;
            virtual void DispatchIndirect() noexcept override;
            virtual void UpdateBuffer(Buffer* buffer, void* data, u64 size) noexcept override;
            virtual void UpdateTexture() noexcept override;

            virtual void CopyBuffer(Buffer* src_buffer, Buffer* dst_buffer) noexcept override;
            virtual void CopyTexture() noexcept override;

            virtual void InsertBarrier(const BarrierDesc& desc) noexcept override;
            virtual void Submit() noexcept override;

            void RequestSecondaryCommandBuffer() noexcept;
            void BeginRenderPass() noexcept;
            
            VkCommandBuffer GetCommandBuffer(CommandQueueType type) noexcept;
            VkCommandBuffer GetSecondaryCommandBuffer() noexcept;
        private:
            void CopyBuffer(VulkanBuffer* src_buffer, VulkanBuffer* dst_buffer) noexcept;
        public:
            VkDevice m_device;
            std::array<VkCommandPool, 3> m_command_pools;
            std::array<VkCommandBuffer, 3> m_command_buffers;
            VkCommandBuffer m_secondary_command_buffer;
        };
    }
}
