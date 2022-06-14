#pragma once

#include <runtime/function/rhi/Buffer.h>
#include "vulkan/ResourceBarrier.h"
namespace Horizon
{
    namespace RHI
    {
		enum CommandQueueType
		{
			GRAPHICS = 0, COMPUTE, TRANSFER
        };
        
        //class CommandQueue{
        //public:
        //    CommandQueueType m_type;
        //    u32 m_index;
        //};
        
        class CommandList {
        public:
            CommandList(CommandQueueType type) noexcept;
            virtual ~CommandList() noexcept;

            virtual void BeginRecording() noexcept = 0;
            virtual void EndRecording() noexcept = 0;

            virtual void BeginRenderPass() noexcept = 0;
            virtual void EndRenderPass() noexcept = 0;
            virtual void Draw() noexcept = 0;
            virtual void DrawIndirect() noexcept = 0;

            virtual void Dispatch() noexcept = 0;
            virtual void DispatchIndirect() noexcept = 0;

            virtual void UpdateBuffer(Buffer* buffer, void* data, u64 size) noexcept = 0;
            virtual void UpdateTexture() noexcept = 0;
            virtual void CopyBuffer(Buffer* dst_buffer, Buffer* src_buffer) noexcept = 0;
            virtual void CopyTexture() noexcept = 0;

            virtual void InsertBarrier(const BarrierDesc& desc) noexcept = 0;
        protected:
			bool is_recoring = false;
			CommandQueueType m_type;

		};

        class CommandContext
        {
        public:
            CommandContext() noexcept;
            CommandContext(const CommandContext& command_list) noexcept = default;
            CommandContext(CommandContext&& command_list) noexcept = default;
            virtual ~CommandContext() noexcept;

        protected:
        private:
        };
    }
}
