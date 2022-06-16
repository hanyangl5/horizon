#pragma once

#include <vulkan/vulkan.h>

#include <array>

#include <runtime/function/rhi/CommandContext.h>
#include <runtime/function/rhi/vulkan/VulkanCommandList.h>

namespace Horizon
{
    namespace RHI
    {
        class VulkanStagePool {
        public:
            VulkanBuffer* GetStageBuffer(const BufferCreateInfo& buffer_create_info) noexcept;
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
            VulkanBuffer* GetStageBuffer(const BufferCreateInfo& buffer_create_info) noexcept;
        private:
            VkDevice m_device;
            // each thread has pools to allocate graphics/compute/transfer commandlist
            std::array<VkCommandPool, 3> m_command_pools{};

			std::array<std::vector<VulkanCommandList*>, 3> m_command_lists{};
			std::array<u32, 3> m_command_lists_count;

            //VulkanStagePool* stage_pool;

        };
    }
}
