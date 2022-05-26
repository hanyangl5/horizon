#pragma once

#include "tiny_gltf.h"

#include <runtime/function/rhi/RenderContext.h>
#include "Device.h"
#include "CommandBuffer.h"

namespace Horizon {



	class Texture : public DescriptorBase
	{  
	public:
		Texture(std::shared_ptr<Device> device, std::shared_ptr<CommandBuffer> command);
		Texture(std::shared_ptr<Device> device, std::shared_ptr<CommandBuffer> command, tinygltf::Image& gltfimage);
		Texture(std::shared_ptr<Device> device, std::shared_ptr<CommandBuffer> command_buffer, TextureCreateInfo create_info);
		~Texture();
		void loadFromFile(const std::string& path, VkImageUsageFlags usage, VkImageLayout layout);
		void transitionImageLayout(VkImageLayout oldLayout, VkImageLayout newLayout);
		void copyBufferToImage(VkBuffer buffer, VkImage image, u32 width, u32 height);
		void createImageView(VkFormat format, VkImageViewType type);
		void createSampler();
		void destroy();
		inline VkImage GetImage() const noexcept { return m_image; }
		inline VkImageSubresourceRange GetSubresourceRange() const noexcept { return subresource_range; }
	private:
		std::shared_ptr<Device> m_device = nullptr;
		std::shared_ptr<CommandBuffer> m_command_buffer = nullptr;
		u8* buffer = nullptr;
		i32 texWidth, texHeight, texChannels;
		u32 mipLevels;
		VkImage m_image;
		VkDeviceMemory m_image_memory;
		VkImageView m_image_view;
		VkSampler m_sampler;
		VkImageSubresourceRange subresource_range;
		VkDescriptorImageInfo mDescriptorImageInfo;
	};

}