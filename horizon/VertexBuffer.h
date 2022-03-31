#pragma once

#include "Device.h"
#include "utils.h"
#include "Vertex.h"
#include "CommandBuffer.h"

namespace Horizon {

	class VertexBuffer
	{
	public:
		VertexBuffer() = default;
		VertexBuffer(std::shared_ptr<Device> device, std::shared_ptr<CommandBuffer> commandBuffer, const std::vector<Vertex>& vertices);
		//VertexBuffer(const VertexBuffer&& rhs);
		//VertexBuffer& operator=(VertexBuffer&& rhs);
		~VertexBuffer();
		VkBuffer get()const;
		u64 getVerticesCount()const;
	private:
		std::shared_ptr<Device> mDevice = nullptr;
		VkBuffer mVertexBuffer;
		VkDeviceMemory mVertexBufferMemory;
		u64 mVerticesCount;
	};
}