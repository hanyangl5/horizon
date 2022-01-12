#pragma once
#include "Vertex.h"
#include "utils.h"
#include "Device.h"
#include <memory>

using Index = u32;
class IndexBuffer {
public:
	IndexBuffer() = default;
	IndexBuffer(std::shared_ptr<Device> device, VkCommandPool cmdpool, const std::vector<Index>&vertices);
	~IndexBuffer();
	VkBuffer get()const;
	u64 getIndicesCount()const;
private:
	VkBuffer mIndexBuffer;
	VkDeviceMemory mIndexBufferMemory;
	std::shared_ptr<Device> mDevice;
	u64 mIndicesCount;
};
