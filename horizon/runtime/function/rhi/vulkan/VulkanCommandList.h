#pragma once

#include <vulkan/vulkan.h>

#include <array>

#include <runtime/function/rhi/CommandContext.h>
#include "VulkanBuffer.h"

namespace Horizon
{
    namespace RHI
    {
        class VulkanCommandContext;

        class VulkanCommandList : public CommandList{
        public:
            VulkanCommandList(CommandQueueType type, VkCommandBuffer command_buffer, const VulkanCommandContext* command_context) noexcept;
            virtual ~VulkanCommandList() noexcept;

            virtual void BeginRecording() noexcept override;
            virtual void EndRecording() noexcept override;

            // graphics commands
            virtual void BeginRenderPass() noexcept override;
            virtual void EndRenderPass() noexcept override;
            virtual void Draw() noexcept override;
            virtual void DrawIndirect() noexcept override;

            // compute commands
            virtual void Dispatch() noexcept override;
            virtual void DispatchIndirect() noexcept override;

            virtual void UpdateBuffer(Buffer* buffer, void* data, u64 size) noexcept override;
			virtual void CopyBuffer(Buffer* src_buffer, Buffer* dst_buffer) noexcept override;
			void CopyBuffer(VulkanBuffer* src_buffer, VulkanBuffer* dst_buffer) noexcept;

            virtual void UpdateTexture() noexcept override;

            virtual void CopyTexture() noexcept override;

            virtual void InsertBarrier(const BarrierDesc& desc) noexcept override;
        public:
            VkCommandBuffer m_command_buffer;
            VulkanCommandContext* m_command_context;
        };

    }
}
