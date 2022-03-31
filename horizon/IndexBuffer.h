#pragma once

#include <vector>
#include <memory>

#include "utils.h"
#include "Device.h"
#include "CommandBuffer.h"

namespace Horizon {

	using Index = u32;
	class IndexBuffer {
	public:
		IndexBuffer() = default;
		IndexBuffer(std::shared_ptr<Device> device, std::shared_ptr<CommandBuffer> commandBuffer, const std::vector<Index>& vertices);
		~IndexBuffer();
		VkBuffer get()const;
		u64 getIndicesCount()const;
	private:
		VkBuffer mIndexBuffer;
		VkDeviceMemory mIndexBufferMemory;
		std::shared_ptr<Device> mDevice = nullptr;
		u64 mIndicesCount;
	};

}