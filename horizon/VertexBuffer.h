#pragma once
#include "Vertex.h"
#include "utils.h"
#include "Device.h"
#include "CommandBuffer.h"
#include <memory>
class VertexBuffer
{
public:
	VertexBuffer() = default;
	VertexBuffer(Device* device, CommandBuffer* commandBuffer, const std::vector<Vertex>& vertices);
	//VertexBuffer(const VertexBuffer&& rhs);
	//VertexBuffer& operator=(VertexBuffer&& rhs);
	~VertexBuffer();
	VkBuffer get()const;
	u64 getVerticesCount()const;
private:
	Device* mDevice = nullptr;
	VkBuffer mVertexBuffer;
	VkDeviceMemory mVertexBufferMemory;
	u64 mVerticesCount;
};
