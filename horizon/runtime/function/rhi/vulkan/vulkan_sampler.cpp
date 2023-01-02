#include "vulkan_sampler.h"

Horizon::Backend::VulkanSampler::VulkanSampler(const VulkanRendererContext &context, const SamplerDesc &desc) noexcept
    : m_context(context) {

    float minSamplerLod = 0;
    float maxSamplerLod = desc.mip_map_mode == MipMapMode::MIPMAP_MODE_LINEAR ? VK_LOD_CLAMP_NONE : 0;

    VkSamplerCreateInfo sampler_create_info{};
    sampler_create_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_create_info.pNext = NULL;
    sampler_create_info.flags = 0;
    sampler_create_info.magFilter = util_to_vk_filter(desc.mag_filter);
    sampler_create_info.minFilter = util_to_vk_filter(desc.min_filter);
    sampler_create_info.mipmapMode = util_to_vk_mip_map_mode(desc.mip_map_mode);
    sampler_create_info.addressModeU = util_to_vk_address_mode(desc.address_u);
    sampler_create_info.addressModeV = util_to_vk_address_mode(desc.address_v);
    sampler_create_info.addressModeW = util_to_vk_address_mode(desc.address_w);
    sampler_create_info.mipLodBias = desc.mMipLodBias;
    sampler_create_info.anisotropyEnable = (desc.mMaxAnisotropy > 0.0f) ? VK_TRUE : VK_FALSE;
    sampler_create_info.maxAnisotropy = desc.mMaxAnisotropy;
    // sampler_create_info.compareEnable =
    //     (gVkComparisonFuncTranslator[pDesc->mCompareFunc] != VK_COMPARE_OP_NEVER) ? VK_TRUE : VK_FALSE;
    // sampler_create_info.compareOp = gVkComparisonFuncTranslator[pDesc->mCompareFunc];
    sampler_create_info.minLod = minSamplerLod;
    sampler_create_info.maxLod = maxSamplerLod;
    sampler_create_info.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    sampler_create_info.unnormalizedCoordinates = VK_FALSE;
    vkCreateSampler(m_context.device, &sampler_create_info, nullptr, &m_sampler);
}

Horizon::Backend::VulkanSampler::~VulkanSampler() noexcept { vkDestroySampler(m_context.device, m_sampler, nullptr); }

VkDescriptorImageInfo *Horizon::Backend::VulkanSampler::GetDescriptorImageInfo() noexcept {
    descriptor_image_info.sampler = m_sampler;
    return &descriptor_image_info;
}
