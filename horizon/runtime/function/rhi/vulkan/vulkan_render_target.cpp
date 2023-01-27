#include "vulkan_render_target.h"
#include "vulkan_texture.h"

Horizon::Backend::VulkanRenderTarget::VulkanRenderTarget(const VulkanRendererContext &context,
                                                     const RenderTargetCreateInfo &render_target_create_info) noexcept
    : RenderTarget(render_target_create_info), m_context(context) {
    TextureCreateInfo create_info{};
    if (rt_type == RenderTargetType::COLOR) {
        create_info.descriptor_types |= DescriptorType::DESCRIPTOR_TYPE_COLOR_ATTACHMENT;
        create_info.descriptor_types |= DescriptorType::DESCRIPTOR_TYPE_RW_TEXTURE |
                                        DescriptorType::DESCRIPTOR_TYPE_TEXTURE; // each rt can be rwtexture and texture
        create_info.texture_format = TextureFormat::TEXTURE_FORMAT_DUMMY_COLOR;
    } else if (rt_type == RenderTargetType::DEPTH_STENCIL) {
        create_info.descriptor_types |= DescriptorType::DESCRIPTOR_TYPE_DEPTH_STENCIL_ATTACHMENT;
        create_info.descriptor_types |= DescriptorType::DESCRIPTOR_TYPE_TEXTURE; // each rt can be rwtexture and texture
    }
    create_info.texture_format = rt_format;
    create_info.texture_type = TextureType::TEXTURE_TYPE_2D;
    create_info.initial_state = ResourceState::RESOURCE_STATE_RENDER_TARGET;
    create_info.width = width;
    create_info.height = height;
    create_info.depth = 1;
    m_texture = Memory::Alloc<VulkanTexture>(m_context, create_info);
}

Horizon::Backend::VulkanRenderTarget::~VulkanRenderTarget() noexcept {
    Memory::Free(m_texture);
    m_texture = nullptr;
}
