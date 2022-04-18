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

#include <runtime/core/log/Log.h>

#include "VulkanBuffer.h"

namespace Horizon {


	Texture::Texture(std::shared_ptr<Device> device, std::shared_ptr<CommandBuffer> command_buffer) :m_device(device), m_command_buffer(command_buffer)
	{
	}

	Texture::Texture(std::shared_ptr<Device> device, std::shared_ptr<CommandBuffer> command_buffer, tinygltf::Image& gltfimage) : m_device(device), m_command_buffer(command_buffer)
	{
		u8* buffer = nullptr;
		VkDeviceSize buffer_size = 0;
		bool deleteBuffer = false;
		if (gltfimage.component == 3) {
			// Most devices don't support RGB only on Vulkan so convert if necessary
			// TODO: Check actual format support and transform only if required
			buffer_size = gltfimage.width * gltfimage.height * 4;
			buffer = new unsigned char[buffer_size];
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
			buffer_size = gltfimage.image.size();

		}
		texWidth = gltfimage.width;
		texHeight = gltfimage.height;

		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		vk_createBuffer(m_device->get(), m_device->getPhysicalDevice(), buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

		void* data;
		vkMapMemory(m_device->get(), stagingBufferMemory, 0, buffer_size, 0, &data);
		memcpy(data, buffer, static_cast<size_t>(buffer_size));
		vkUnmapMemory(m_device->get(), stagingBufferMemory);

		//stbi_image_free(buffer);

		// create image
		VkImageCreateInfo image_create_info{};
		image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		image_create_info.imageType = VK_IMAGE_TYPE_2D;
		image_create_info.extent.width = static_cast<uint32_t>(texWidth);
		image_create_info.extent.height = static_cast<uint32_t>(texHeight);
		image_create_info.extent.depth = 1;
		image_create_info.mipLevels = 1;
		image_create_info.arrayLayers = 1;
		image_create_info.format = VK_FORMAT_R8G8B8A8_UNORM;
		image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
		image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		image_create_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;

		CHECK_VK_RESULT(vkCreateImage(m_device->get(), &image_create_info, nullptr, &m_image));

		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(m_device->get(), m_image, &memRequirements);

		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = findMemoryType(m_device->getPhysicalDevice(), memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		if (vkAllocateMemory(m_device->get(), &allocInfo, nullptr, &m_image_memory) != VK_SUCCESS) {
			throw std::runtime_error("failed to allocate image memory!");
		}

		vkBindImageMemory(m_device->get(), m_image, m_image_memory, 0);

		transitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		copyBufferToImage(stagingBuffer, m_image, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
		transitionImageLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		vkDestroyBuffer(m_device->get(), stagingBuffer, nullptr);
		vkFreeMemory(m_device->get(), stagingBufferMemory, nullptr);

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

		vkCreateSampler(m_device->get(), &samplerInfo, nullptr, &m_sampler);


		// fill descriptor info
		imageDescriptorInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageDescriptorInfo.imageView = m_image_view;
		imageDescriptorInfo.sampler = m_sampler;
	}

	Texture::~Texture()
	{
		vkDestroyImage(m_device->get(), m_image, nullptr);
		vkDestroyImageView(m_device->get(), m_image_view, nullptr);
		vkDestroySampler(m_device->get(), m_sampler, nullptr);
		vkFreeMemory(m_device->get(), m_image_memory, nullptr);
	}

	void Texture::loadFromFile(const std::string& path, VkImageUsageFlags usage, VkImageLayout layout) {
		buffer = stbi_load(path.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
		texChannels = 4;
		VkDeviceSize imageSize = texWidth * texHeight * texChannels;

		if (!buffer) {
			LOG_ERROR("failed to load texture image {}", path);
			return;
		}

		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		vk_createBuffer(m_device->get(), m_device->getPhysicalDevice(), imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

		void* data;
		vkMapMemory(m_device->get(), stagingBufferMemory, 0, imageSize, 0, &data);
		memcpy(data, buffer, static_cast<size_t>(imageSize));
		vkUnmapMemory(m_device->get(), stagingBufferMemory);

		stbi_image_free(buffer);

		// create image
		VkImageCreateInfo image_create_info{};
		image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		image_create_info.imageType = VK_IMAGE_TYPE_2D;
		image_create_info.extent.width = static_cast<uint32_t>(texWidth);
		image_create_info.extent.height = static_cast<uint32_t>(texHeight);
		image_create_info.extent.depth = 1;
		image_create_info.mipLevels = 1;
		image_create_info.arrayLayers = 1;
		image_create_info.format = VK_FORMAT_R8G8B8A8_UNORM;
		image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
		image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		image_create_info.usage = usage;
		image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;

		CHECK_VK_RESULT(vkCreateImage(m_device->get(), &image_create_info, nullptr, &m_image));

		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(m_device->get(), m_image, &memRequirements);

		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = findMemoryType(m_device->getPhysicalDevice(), memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		if (vkAllocateMemory(m_device->get(), &allocInfo, nullptr, &m_image_memory) != VK_SUCCESS) {
			throw std::runtime_error("failed to allocate image memory!");
		}

		vkBindImageMemory(m_device->get(), m_image, m_image_memory, 0);

		transitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		copyBufferToImage(stagingBuffer, m_image, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
		transitionImageLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, layout);

		vkDestroyBuffer(m_device->get(), stagingBuffer, nullptr);
		vkFreeMemory(m_device->get(), stagingBufferMemory, nullptr);

		createImageView(VK_FORMAT_R8G8B8A8_UNORM);
		createSampler();

		// fill descriptor info
		imageDescriptorInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageDescriptorInfo.imageView = m_image_view;
		imageDescriptorInfo.sampler = m_sampler;

	}

	void Texture::loadFromglTfImage(tinygltf::Image& gltfimage, std::shared_ptr<Device> device, std::shared_ptr<CommandBuffer> command)
	{
		//	m_device = device;
		//	m_command_buffer = command;
		//	u8* buffer = nullptr;
		//	VkDeviceSize buffer_size = 0;
		//	bool deleteBuffer = false;
		//	if (gltfimage.component == 3) {
		//		// Most devices don't support RGB only on Vulkan so convert if necessary
		//		// TODO: Check actual format support and transform only if required
		//		buffer_size = gltfimage.width * gltfimage.height * 4;
		//		buffer = new unsigned char[buffer_size];
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
		//		buffer_size = gltfimage.image.size();
		//
		//	}
		//	texWidth = gltfimage.width;
		//	texHeight = gltfimage.height;
		//
		//	VkBuffer stagingBuffer;
		//	VkDeviceMemory stagingBufferMemory;
		//	vk_createBuffer(m_device->get(), m_device->getPhysicalDevice(), buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
		//
		//	void* data;
		//	vkMapMemory(m_device->get(), stagingBufferMemory, 0, buffer_size, 0, &data);
		//	memcpy(data, buffer, static_cast<size_t>(buffer_size));
		//	vkUnmapMemory(m_device->get(), stagingBufferMemory);
		//
		//	//stbi_image_free(buffer);
		//
		//	// create image
		//	VkImageCreateInfo image_create_info{};
		//	image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		//	image_create_info.imageType = VK_IMAGE_TYPE_2D;
		//	image_create_info.extent.width = static_cast<uint32_t>(texWidth);
		//	image_create_info.extent.height = static_cast<uint32_t>(texHeight);
		//	image_create_info.extent.depth = 1;
		//	image_create_info.mipLevels = 1;
		//	image_create_info.arrayLayers = 1;
		//	image_create_info.format = VK_FORMAT_R8G8B8A8_UNORM;
		//	image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
		//	image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		//	image_create_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		//	image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		//	image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
		//
		//	CHECK_VK_RESULT(vkCreateImage(m_device->get(), &image_create_info, nullptr, &m_image));
		//
		//	VkMemoryRequirements memRequirements;
		//	vkGetImageMemoryRequirements(m_device->get(), m_image, &memRequirements);
		//
		//	VkMemoryAllocateInfo allocInfo{};
		//	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		//	allocInfo.allocationSize = memRequirements.size;
		//	allocInfo.memoryTypeIndex = findMemoryType(m_device->getPhysicalDevice(), memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		//
		//	if (vkAllocateMemory(m_device->get(), &allocInfo, nullptr, &m_image_memory) != VK_SUCCESS) {
		//		throw std::runtime_error("failed to allocate image memory!");
		//	}
		//
		//	vkBindImageMemory(m_device->get(), m_image, m_image_memory, 0);
		//
		//	transitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		//	copyBufferToImage(stagingBuffer, m_image, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
		//	transitionImageLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		//
		//	vkDestroyBuffer(m_device->get(), stagingBuffer, nullptr);
		//	vkFreeMemory(m_device->get(), stagingBufferMemory, nullptr);
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
		//	vkCreateSampler(m_device->get(), &samplerInfo, nullptr, &m_sampler);
	}


	// VkDescriptorImageInfo Texture::getDescriptor()
	// {
	// 	mDescriptorImageInfo.imageView = m_image_view;
	// 	mDescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	// 	mDescriptorImageInfo.sampler = m_sampler;
	// 	return mDescriptorImageInfo;
	// }

	void Texture::transitionImageLayout(VkImageLayout oldLayout, VkImageLayout newLayout)
	{
		VkCommandBuffer cmdbuf = m_command_buffer->beginSingleTimeCommands();

		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = oldLayout;
		barrier.newLayout = newLayout;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = m_image;
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

		m_command_buffer->endSingleTimeCommands(cmdbuf);
	}

	void Texture::copyBufferToImage(VkBuffer buffer, VkImage image, u32 width, u32 height) {
		VkCommandBuffer cmdbuf = m_command_buffer->beginSingleTimeCommands();

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

		m_command_buffer->endSingleTimeCommands(cmdbuf);
	}

	void Texture::createImageView(VkFormat format) {
		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = m_image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = format;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;

		CHECK_VK_RESULT(vkCreateImageView(m_device->get(), &viewInfo, nullptr, &m_image_view));
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

		vkCreateSampler(m_device->get(), &samplerInfo, nullptr, &m_sampler);
	}

	void Texture::destroy()
	{
		vkDestroyImageView(m_device->get(), m_image_view, nullptr);
		vkDestroyImage(m_device->get(), m_image, nullptr);
		vkFreeMemory(m_device->get(), m_image_memory, nullptr);
	}
}
