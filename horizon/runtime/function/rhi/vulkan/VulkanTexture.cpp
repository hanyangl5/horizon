#include <runtime/function/rhi/vulkan/VulkanTexture.h>
#include <runtime/function/rhi/vulkan/VulkanUtils.h>

namespace Horizon::RHI {

VulkanTexture::VulkanTexture(const VulkanRendererContext &context,
                             const TextureCreateInfo &texture_create_info) noexcept
    : Texture(texture_create_info), m_context(context) {

    VkImageCreateInfo image_create_info{};
    image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_create_info.imageType = ToVkImageType(texture_create_info.texture_type);
    image_create_info.extent.width = texture_create_info.width;
    image_create_info.extent.height = texture_create_info.height;
    image_create_info.extent.depth = texture_create_info.depth;
    image_create_info.mipLevels = 1;
    image_create_info.arrayLayers = 1;
    image_create_info.format = ToVkImageFormat(texture_create_info.texture_format);
    image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    image_create_info.usage = util_to_vk_image_usage(texture_create_info.descriptor_type);
    image_create_info.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;

    VmaAllocationCreateInfo allocation_creat_info{};
    allocation_creat_info.usage = VMA_MEMORY_USAGE_AUTO;

    CHECK_VK_RESULT(vmaCreateImage(m_context.vma_allocator, &image_create_info, &allocation_creat_info, &m_image,
                                   &m_memory, nullptr));

    if (texture_create_info.initial_state == ResourceState::RESOURCE_STATE_RENDER_TARGET ||
        texture_create_info.initial_state==ResourceState::RESOURCE_STATE_SHADER_RESOURCE) {
        VkImageViewCreateInfo image_view_create_info{};
        image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        image_view_create_info.image = m_image;
        image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        image_view_create_info.format = ToVkImageFormat(texture_create_info.texture_format);
        image_view_create_info.subresourceRange = {};
        image_view_create_info.subresourceRange.aspectMask = ToVkAspectMaskFlags(image_view_create_info.format, false);
        image_view_create_info.subresourceRange.baseMipLevel = 0;
        image_view_create_info.subresourceRange.levelCount = 1;
        image_view_create_info.subresourceRange.baseArrayLayer = 0;
        image_view_create_info.subresourceRange.layerCount = 1;

        CHECK_VK_RESULT(vkCreateImageView(m_context.device, &image_view_create_info, nullptr, &m_image_view));
    }
}

VulkanTexture::~VulkanTexture() noexcept {
    vmaDestroyImage(m_context.vma_allocator, m_image, m_memory);
    if (m_image_view != VK_NULL_HANDLE) {
        vkDestroyImageView(m_context.device, m_image_view, nullptr);
    }
}

} // namespace Horizon::RHI
