#pragma once

#include "tiny_gltf.h"

#include "utils.h"
#include "Device.h"
#include "CommandBuffer.h"

namespace Horizon {

	class Texture : public DescriptorBase
	{
	public:
		Texture(Device* device, CommandBuffer* command);
		Texture(Device* device, CommandBuffer* command, tinygltf::Image& gltfimage);
		~Texture();
		void loadFromFile(const std::string& path, VkImageUsageFlags usage, VkImageLayout layout);
		void loadFromglTfImage(tinygltf::Image& gltfimage, Device* device, CommandBuffer* command);
		VkDescriptorImageInfo getDescriptor();
		void transitionImageLayout(VkImageLayout oldLayout, VkImageLayout newLayout);
		void copyBufferToImage(VkBuffer buffer, VkImage image, u32 width, u32 height);
		void createImageView(VkFormat format);
		void createSampler();
		void destroy();
	private:
		Device* mDevice = nullptr;
		CommandBuffer* mCommandBuffer = nullptr;
		u8* buffer = nullptr;
		i32 texWidth, texHeight, texChannels;
		u32 mipLevels;
		VkImage mImage;
		VkDeviceMemory mImageMemory;
		VkImageView mImageView;
		VkSampler mSampler;
		VkDescriptorImageInfo mDescriptorImageInfo;
	};

}