#include "Texture.h"

#include <vulkan/vulkan.hpp>
#ifndef TINYGLTF_IMPLEMENTATION
#define TINYGLTF_IMPLEMENTATION
#endif // !TINYGLTF_IMPLEMENTATION

#ifndef STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#endif

#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#endif // !STB_IMAGE_IMPLEMENTATION

#include "tiny_gltf.h"

#include "VulkanBuffer.h"

namespace Horizon {


	Texture::Texture(Device* device, CommandBuffer* commandBuffer) :mDevice(device), mCommandBuffer(commandBuffer)
	{
	}

	Texture::Texture(Device* device, CommandBuffer* commandBuffer, tinygltf::Image& gltfimage) : mDevice(device), mCommandBuffer(commandBuffer)
	{
		u8* buffer = nullptr;
		VkDeviceSize bufferSize = 0;
		bool deleteBuffer = false;
		if (gltfimage.component == 3) {
			// Most devices don't support RGB only on Vulkan so convert if necessary
			// TODO: Check actual format support and transform only if required
			bufferSize = gltfimage.width * gltfimage.height * 4;
			buffer = new unsigned char[bufferSize];
			unsigned char* rgba = buffer;
			unsigned char* rgb = &gltfimage.image[0];
			for (int32_t i = 0; i < gltfimage.width * gltfimage.height; ++i) {
				for (int32_t j = 0; j < 3; ++j) {
					rgba[j] = rgb[j];
				}
				rgba += 4;
				rgb += 3;
			}
			deleteBuffer = true;
		}
		else {
			buffer = &gltfimage.image[0];
			bufferSize = gltfimage.image.size();

		}
		texWidth = gltfimage.width;
		texHeight = gltfimage.height;

		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		vk_createBuffer(mDevice->get(), mDevice->getPhysicalDevice(), bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

		void* data;
		vkMapMemory(mDevice->get(), stagingBufferMemory, 0, bufferSize, 0, &data);
		memcpy(data, buffer, static_cast<size_t>(bufferSize));
		vkUnmapMemory(mDevice->get(), stagingBufferMemory);

		//stbi_image_free(buffer);

		// create image
		VkImageCreateInfo imageCreateInfo{};
		imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.extent.width = static_cast<uint32_t>(texWidth);
		imageCreateInfo.extent.height = static_cast<uint32_t>(texHeight);
		imageCreateInfo.extent.depth = 1;
		imageCreateInfo.mipLevels = 1;
		imageCreateInfo.arrayLayers = 1;
		imageCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;

		printVkError(vkCreateImage(mDevice->get(), &imageCreateInfo, nullptr, &mImage));

		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(mDevice->get(), mImage, &memRequirements);

		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = findMemoryType(mDevice->getPhysicalDevice(), memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		if (vkAllocateMemory(mDevice->get(), &allocInfo, nullptr, &mImageMemory) != VK_SUCCESS) {
			throw std::runtime_error("failed to allocate image memory!");
		}

		vkBindImageMemory(mDevice->get(), mImage, mImageMemory, 0);

		transitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		copyBufferToImage(stagingBuffer, mImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
		transitionImageLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		vkDestroyBuffer(mDevice->get(), stagingBuffer, nullptr);
		vkFreeMemory(mDevice->get(), stagingBufferMemory, nullptr);

		createImageView(VK_FORMAT_R8G8B8A8_UNORM);

		VkSamplerCreateInfo samplerInfo{};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.anisotropyEnable = VK_FALSE;
		samplerInfo.maxAnisotropy = 1.0;
		samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		samplerInfo.unnormalizedCoordinates = VK_FALSE;
		samplerInfo.compareEnable = VK_FALSE;
		samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

		vkCreateSampler(mDevice->get(), &samplerInfo, nullptr, &mSampler);


		// fill descriptor info
		imageDescriptorInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageDescriptorInfo.imageView = mImageView;
		imageDescriptorInfo.sampler = mSampler;
	}

	Texture::~Texture()
	{
	}

	void Texture::loadFromFile(const std::string& path, VkImageUsageFlags usage, VkImageLayout layout) {
		buffer = stbi_load(path.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
		VkDeviceSize imageSize = texWidth * texHeight * texChannels;

		if (!buffer) {
			throw std::runtime_error("failed to load texture image!");
		}

		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		vk_createBuffer(mDevice->get(), mDevice->getPhysicalDevice(), imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

		void* data;
		vkMapMemory(mDevice->get(), stagingBufferMemory, 0, imageSize, 0, &data);
		memcpy(data, buffer, static_cast<size_t>(imageSize));
		vkUnmapMemory(mDevice->get(), stagingBufferMemory);

		stbi_image_free(buffer);

		// create image
		VkImageCreateInfo imageCreateInfo{};
		imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.extent.width = static_cast<uint32_t>(texWidth);
		imageCreateInfo.extent.height = static_cast<uint32_t>(texHeight);
		imageCreateInfo.extent.depth = 1;
		imageCreateInfo.mipLevels = 1;
		imageCreateInfo.arrayLayers = 1;
		imageCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.initialLayout = layout;
		imageCreateInfo.usage = usage;
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;

		printVkError(vkCreateImage(mDevice->get(), &imageCreateInfo, nullptr, &mImage));

		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(mDevice->get(), mImage, &memRequirements);

		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = findMemoryType(mDevice->getPhysicalDevice(), memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		if (vkAllocateMemory(mDevice->get(), &allocInfo, nullptr, &mImageMemory) != VK_SUCCESS) {
			throw std::runtime_error("failed to allocate image memory!");
		}

		vkBindImageMemory(mDevice->get(), mImage, mImageMemory, 0);

		transitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		copyBufferToImage(stagingBuffer, mImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
		transitionImageLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		vkDestroyBuffer(mDevice->get(), stagingBuffer, nullptr);
		vkFreeMemory(mDevice->get(), stagingBufferMemory, nullptr);

		createImageView(VK_FORMAT_R8G8B8A8_UNORM);
		createSampler();

	}

	void Texture::loadFromglTfImage(tinygltf::Image& gltfimage, Device* device, CommandBuffer* command)
	{
		//	mDevice = device;
		//	mCommandBuffer = command;
		//	u8* buffer = nullptr;
		//	VkDeviceSize bufferSize = 0;
		//	bool deleteBuffer = false;
		//	if (gltfimage.component == 3) {
		//		// Most devices don't support RGB only on Vulkan so convert if necessary
		//		// TODO: Check actual format support and transform only if required
		//		bufferSize = gltfimage.width * gltfimage.height * 4;
		//		buffer = new unsigned char[bufferSize];
		//		unsigned char* rgba = buffer;
		//		unsigned char* rgb = &gltfimage.image[0];
		//		for (int32_t i = 0; i < gltfimage.width * gltfimage.height; ++i) {
		//			for (int32_t j = 0; j < 3; ++j) {
		//				rgba[j] = rgb[j];
		//			}
		//			rgba += 4;
		//			rgb += 3;
		//		}
		//		deleteBuffer = true;
		//	}
		//	else {
		//		buffer = &gltfimage.image[0];
		//		bufferSize = gltfimage.image.size();
		//
		//	}
		//	texWidth = gltfimage.width;
		//	texHeight = gltfimage.height;
		//
		//	VkBuffer stagingBuffer;
		//	VkDeviceMemory stagingBufferMemory;
		//	vk_createBuffer(mDevice->get(), mDevice->getPhysicalDevice(), bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
		//
		//	void* data;
		//	vkMapMemory(mDevice->get(), stagingBufferMemory, 0, bufferSize, 0, &data);
		//	memcpy(data, buffer, static_cast<size_t>(bufferSize));
		//	vkUnmapMemory(mDevice->get(), stagingBufferMemory);
		//
		//	//stbi_image_free(buffer);
		//
		//	// create image
		//	VkImageCreateInfo imageCreateInfo{};
		//	imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		//	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		//	imageCreateInfo.extent.width = static_cast<uint32_t>(texWidth);
		//	imageCreateInfo.extent.height = static_cast<uint32_t>(texHeight);
		//	imageCreateInfo.extent.depth = 1;
		//	imageCreateInfo.mipLevels = 1;
		//	imageCreateInfo.arrayLayers = 1;
		//	imageCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
		//	imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		//	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		//	imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		//	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		//	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		//
		//	printVkError(vkCreateImage(mDevice->get(), &imageCreateInfo, nullptr, &mImage));
		//
		//	VkMemoryRequirements memRequirements;
		//	vkGetImageMemoryRequirements(mDevice->get(), mImage, &memRequirements);
		//
		//	VkMemoryAllocateInfo allocInfo{};
		//	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		//	allocInfo.allocationSize = memRequirements.size;
		//	allocInfo.memoryTypeIndex = findMemoryType(mDevice->getPhysicalDevice(), memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		//
		//	if (vkAllocateMemory(mDevice->get(), &allocInfo, nullptr, &mImageMemory) != VK_SUCCESS) {
		//		throw std::runtime_error("failed to allocate image memory!");
		//	}
		//
		//	vkBindImageMemory(mDevice->get(), mImage, mImageMemory, 0);
		//
		//	transitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		//	copyBufferToImage(stagingBuffer, mImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
		//	transitionImageLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		//
		//	vkDestroyBuffer(mDevice->get(), stagingBuffer, nullptr);
		//	vkFreeMemory(mDevice->get(), stagingBufferMemory, nullptr);
		//
		//	createImageView(VK_FORMAT_R8G8B8A8_UNORM);
		//
		//	VkSamplerCreateInfo samplerInfo{};
		//	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		//	samplerInfo.magFilter = VK_FILTER_LINEAR;
		//	samplerInfo.minFilter = VK_FILTER_LINEAR;
		//	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		//	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		//	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		//	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		//	samplerInfo.anisotropyEnable = VK_FALSE;
		//	samplerInfo.maxAnisotropy = 1.0;
		//	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		//	samplerInfo.unnormalizedCoordinates = VK_FALSE;
		//	samplerInfo.compareEnable = VK_FALSE;
		//	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
		//	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		//
		//	vkCreateSampler(mDevice->get(), &samplerInfo, nullptr, &mSampler);
	}


	VkDescriptorImageInfo Texture::getDescriptor()
	{
		mDescriptorImageInfo.imageView = mImageView;
		mDescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		mDescriptorImageInfo.sampler = mSampler;
		return mDescriptorImageInfo;
	}

	void Texture::transitionImageLayout(VkImageLayout oldLayout, VkImageLayout newLayout)
	{
		VkCommandBuffer cmdbuf = mCommandBuffer->beginSingleTimeCommands();

		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = oldLayout;
		barrier.newLayout = newLayout;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = mImage;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;

		VkPipelineStageFlags sourceStage;
		VkPipelineStageFlags destinationStage;

		if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

			sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		}
		else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		}
		else {
			throw std::invalid_argument("unsupported layout transition!");
		}

		vkCmdPipelineBarrier(
			cmdbuf,
			sourceStage, destinationStage,
			0,
			0, nullptr,
			0, nullptr,
			1, &barrier
		);

		mCommandBuffer->endSingleTimeCommands(cmdbuf);
	}

	void Texture::copyBufferToImage(VkBuffer buffer, VkImage image, u32 width, u32 height) {
		VkCommandBuffer cmdbuf = mCommandBuffer->beginSingleTimeCommands();

		VkBufferImageCopy region{};
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;
		region.imageOffset = { 0, 0, 0 };
		region.imageExtent = {
			width,
			height,
			1
		};

		vkCmdCopyBufferToImage(cmdbuf, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

		mCommandBuffer->endSingleTimeCommands(cmdbuf);
	}

	void Texture::createImageView(VkFormat format) {
		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = mImage;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = format;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;

		printVkError(vkCreateImageView(mDevice->get(), &viewInfo, nullptr, &mImageView));
	}

	void Texture::createSampler()
	{
		VkSamplerCreateInfo samplerInfo{};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.anisotropyEnable = VK_FALSE;
		samplerInfo.maxAnisotropy = 1.0;
		samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		samplerInfo.unnormalizedCoordinates = VK_FALSE;
		samplerInfo.compareEnable = VK_FALSE;
		samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

		vkCreateSampler(mDevice->get(), &samplerInfo, nullptr, &mSampler);
	}

	void Texture::destroy()
	{
		vkDestroyImageView(mDevice->get(), mImageView, nullptr);
		vkDestroyImage(mDevice->get(), mImage, nullptr);
		vkFreeMemory(mDevice->get(), mImageMemory, nullptr);
	}
}
