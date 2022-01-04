#pragma once
#include "Vertex.h"
#include "utils.h"
#include "Device.h"
#include <memory>
class VertexBuffer
{
public:
	VertexBuffer()=default;
	VertexBuffer(std::shared_ptr<Device> device,const std::vector<Vertex>& vertices);
	//VertexBuffer(const VertexBuffer&& rhs);
	//VertexBuffer& operator=(VertexBuffer&& rhs);
	~VertexBuffer();
	VkBuffer get()const;
	u64 getVerticesCount()const;
private:
	std::shared_ptr<Device> mDevice;
	VkBuffer mVertexBuffer;
	VkDeviceMemory mVertexBufferMemory;
	u64 mVerticesCount;
};
