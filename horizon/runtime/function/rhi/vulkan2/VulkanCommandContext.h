#pragma once

#include <runtime/function/rhi/CommandContext.h>
#include "VulkanBuffer2.h"

namespace Horizon
{
    namespace RHI
    {

        class VulkanCommandList : public CommandList{
        public:
            VulkanCommandList(CommandQueueType type, VkCommandBuffer command_buffer): m_type(type){

            }

            void BeginRecoridng() noexcept override;
            void EndRecording() noexcept override;

            // graphics commands
            void BeginRenderPass() noexcept override;
            void EndRenderPass() noexcept override;
            void Draw() noexcept override;
            void DrawIndirect() noexcept override;

            // compute commands
            void Dispatch() noexcept override;
            void DispatchIndirect() noexcept override;

            void UpdateBuffer(Buffer* buffer, void* data, u64 size) noexcept override;
			void CopyBuffer(Buffer* src_buffer, Buffer* dst_buffer) noexcept override;
			void CopyBuffer(VulkanBuffer* src_buffer, VulkanBuffer* dst_buffer) noexcept;
            void UpdateBuffer(Buffer* buffer, void* data, u64 size) noexcept override;

            void UpdateTexture() noexcept override;

            void CopyTexture() noexcept override;

			void InsertBarrier(const BarrierDesc& desc) noexcept override;
        private:
            VkCommandBuffer m_command_buffer;
        };

        class VulkanCommandContext : public CommandContext
        {
        public:
            VulkanCommandContext(VkDevice device) noexcept;
            ~VulkanCommandContext() noexcept;
            VulkanCommandList GetVulkanCommandList(CommandQueueType type) noexcept
        public:
            VkDevice m_device;
            std::array<VkCommandPool, 3> m_command_pools;
        };
    }
}
