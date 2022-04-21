#pragma once

#include <vector>
#include <memory>

#include <runtime/function/render/RenderContext.h>
#include "Device.h"
#include "CommandBuffer.h"

namespace Horizon {

	using Index = u32;
	class IndexBuffer {
	public:
		IndexBuffer() = default;
		IndexBuffer(std::shared_ptr<Device> device, std::shared_ptr<CommandBuffer> command_buffer, const std::vector<Index>& vertices);
		~IndexBuffer();
		VkBuffer Get()const noexcept;
		u64 getIndicesCount()const noexcept;
	private:
		VkBuffer m_index_buffer;
		VkDeviceMemory m_index_buffer_memory;
		std::shared_ptr<Device> m_device = nullptr;
		u64 m_indices_count;
	};

}