#pragma once

#include "Device.h"
#include <runtime/function/rhi/RenderContext.h>
#include "Vertex.h"
#include "CommandBuffer.h"

namespace Horizon {

	class VertexBuffer
	{
	public:
		VertexBuffer() = default;
		VertexBuffer(std::shared_ptr<Device> device, std::shared_ptr<CommandBuffer> command_buffer, const std::vector<Vertex>& vertices);
		//VertexBuffer(const VertexBuffer&& rhs);
		//VertexBuffer& operator=(VertexBuffer&& rhs);
		~VertexBuffer();
		VkBuffer Get()const noexcept;
		u64 getVerticesCount()const noexcept;
	private:
		std::shared_ptr<Device> m_device = nullptr;
		VkBuffer m_vertex_buffer;
		VkDeviceMemory m_vertex_buffer_memory;
		u64 m_vertices_count;
	};
}