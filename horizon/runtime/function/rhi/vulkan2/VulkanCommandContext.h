#pragma once

#include <vulkan/vulkan.h>

#include <array>

#include <runtime/function/rhi/CommandContext.h>
#include "VulkanBuffer2.h"

namespace Horizon
{
    namespace RHI
    {

        class VulkanCommandList : public CommandList{
        public:
            VulkanCommandList(CommandQueueType type, VkCommandBuffer command_buffer) noexcept;
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
        private:
            void CheckStatus() noexcept;
        private:
            VkCommandBuffer m_command_buffer;
        };

        class VulkanCommandContext : public CommandContext
        {
        public:
            VulkanCommandContext(VkDevice device) noexcept;            
            VulkanCommandContext(const VulkanCommandContext& command_list) noexcept = default;
            VulkanCommandContext(VulkanCommandContext&& command_list) noexcept = default;
            virtual ~VulkanCommandContext() noexcept override;
            VulkanCommandList* GetVulkanCommandList(CommandQueueType type) noexcept;
            virtual void Reset() noexcept override;
        private:
            VkDevice m_device;
            // each thread has pools to allocate graphics/compute/transfer commandlist
            std::array<VkCommandPool, 3> m_command_pools{};

            std::array<std::vector<VkCommandBuffer>, 3> m_command_lists1{};
            std::array<u32, 3> m_command_lists_count;
        };
    }
}
