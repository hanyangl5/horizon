#pragma once

#include "tiny_gltf.h"

#include "utils.h"
#include "Device.h"
#include "CommandBuffer.h"

namespace Horizon {

	class Texture : public DescriptorBase
	{
	public:
		Texture(std::shared_ptr<Device> device, std::shared_ptr<CommandBuffer> command);
		Texture(std::shared_ptr<Device> device, std::shared_ptr<CommandBuffer> command, tinygltf::Image& gltfimage);
		~Texture();
		void loadFromFile(const std::string& path, VkImageUsageFlags usage, VkImageLayout layout);
		void loadFromglTfImage(tinygltf::Image& gltfimage, std::shared_ptr<Device> device, std::shared_ptr<CommandBuffer> command);
		void transitionImageLayout(VkImageLayout oldLayout, VkImageLayout newLayout);
		void copyBufferToImage(VkBuffer buffer, VkImage image, u32 width, u32 height);
		void createImageView(VkFormat format);
		void createSampler();
		void destroy();
	private:
		std::shared_ptr<Device> mDevice = nullptr;
		std::shared_ptr<CommandBuffer> mCommandBuffer = nullptr;
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