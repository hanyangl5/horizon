#pragma once
#include "Device.h"
#include <string>
#include "utils.h"
#include "CommandBuffer.h"

class Texture
{
public:
	Texture(Device* device, CommandBuffer* command);
	~Texture();
	void loadFromFile(const std::string& path, VkImageUsageFlags usage, VkImageLayout layout);
	void loadFromBuffer(void* buffer, VkDeviceSize bufferSize, VkFormat format, uint32_t texWidth, uint32_t texHeight, Device* device,
		VkFilter filter = VK_FILTER_LINEAR,
		VkImageUsageFlags imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
		VkImageLayout imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	VkDescriptorImageInfo getDescriptor();
	void transitionImageLayout(VkImageLayout oldLayout, VkImageLayout newLayout);
	void copyBufferToImage(VkBuffer buffer, VkImage image, u32 width, u32 height);
	void createImageView(VkFormat format);
	void createSampler();
private:
	Device* mDevice = nullptr;
	CommandBuffer* mCommandBuffer = nullptr;
	u8* buffer = nullptr;
	int texWidth, texHeight, texChannels;
	VkImage mImage;
	VkDeviceMemory mImageMemory;
	VkImageView mImageView;
	VkSampler mSampler;
	VkDescriptorImageInfo mDescriptorImageInfo;
};

