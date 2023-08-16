#include "VulkanTexture.h"
#include <runtime/core/memory/Memory.h>
namespace Horizon::Backend {

VulkanTexture::VulkanTexture(const VulkanRendererContext &context,
                             const TextureCreateInfo &texture_create_info) noexcept
    : Texture(texture_create_info), m_context(context) {
    if (texture_create_info.texture_format == TextureFormat::TEXTURE_FORMAT_UNDEFINED ||
        texture_create_info.texture_format == TextureFormat::TEXTURE_FORMAT_DUMMY_COLOR)
        return;
    VkImageCreateInfo image_create_info{};
    image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_create_info.imageType = ToVkImageType(texture_create_info.texture_type);
    image_create_info.extent.width = m_width;
    image_create_info.extent.height = m_height;
    image_create_info.extent.depth = m_depth;
    image_create_info.mipLevels = mip_map_level;
    image_create_info.arrayLayers = 1;
    image_create_info.format = ToVkImageFormat(m_format);
    image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    image_create_info.usage |= util_to_vk_image_usage(m_descriptor_types);
    image_create_info.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_create_info.arrayLayers = texture_create_info.array_layer;
    if (texture_create_info.texture_type == TextureType::TEXTURE_TYPE_CUBE) {
        image_create_info.arrayLayers = 6;
        image_create_info.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    }

    VmaAllocationCreateInfo allocation_creat_info{};
    allocation_creat_info.usage = VMA_MEMORY_USAGE_AUTO;

    CHECK_VK_RESULT(vmaCreateImage(m_context.vma_allocator, &image_create_info, &allocation_creat_info, &m_image,
                                   &m_memory, nullptr));

    if (texture_create_info.initial_state == ResourceState::RESOURCE_STATE_RENDER_TARGET ||
        texture_create_info.initial_state == ResourceState::RESOURCE_STATE_SHADER_RESOURCE ||
        texture_create_info.initial_state == ResourceState::RESOURCE_STATE_UNORDERED_ACCESS) {
        VkImageViewCreateInfo image_view_create_info{};
        image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        image_view_create_info.image = m_image;
        image_view_create_info.viewType = ToVkImageViewType(texture_create_info.texture_type);
        image_view_create_info.format = ToVkImageFormat(texture_create_info.texture_format);
        image_view_create_info.subresourceRange = {};
        image_view_create_info.subresourceRange.aspectMask = ToVkAspectMaskFlags(image_view_create_info.format, false);
        image_view_create_info.subresourceRange.baseMipLevel = 0;
        image_view_create_info.subresourceRange.levelCount = mip_map_level;
        image_view_create_info.subresourceRange.baseArrayLayer = 0;
        image_view_create_info.subresourceRange.layerCount = texture_create_info.array_layer;

        CHECK_VK_RESULT(vkCreateImageView(m_context.device, &image_view_create_info, nullptr, &m_image_view));
    }
}

VulkanTexture::~VulkanTexture() noexcept {
    if (m_stage_buffer != nullptr) {
        Memory::Free<VulkanBuffer>(m_stage_buffer);
    }
    vmaDestroyImage(m_context.vma_allocator, m_image, m_memory);
    if (m_image_view != VK_NULL_HANDLE) {
        vkDestroyImageView(m_context.device, m_image_view, nullptr);
    }
}

VkDescriptorImageInfo *VulkanTexture::GetDescriptorImageInfo(DescriptorType descriptor_type) noexcept {
    descriptor_image_info.imageView = m_image_view;
    if (DescriptorType::DESCRIPTOR_TYPE_RW_TEXTURE == (descriptor_type & DescriptorType::DESCRIPTOR_TYPE_RW_TEXTURE)) {
        descriptor_image_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    } else if (DescriptorType::DESCRIPTOR_TYPE_TEXTURE == (descriptor_type & DescriptorType::DESCRIPTOR_TYPE_TEXTURE)) {
        descriptor_image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
    return &descriptor_image_info;
}

} // namespace Horizon::Backend
