#include "VulkanRenderTarget.h"
#include "VulkanTexture.h"

Horizon::RHI::VulkanRenderTarget::VulkanRenderTarget(const VulkanRendererContext &context,
                                                     const RenderTargetCreateInfo &render_target_create_info) noexcept
    : RenderTarget(render_target_create_info), m_context(context) {
    TextureCreateInfo create_info{};
    if (render_target_create_info.rt_type == RenderTargetType::COLOR) {
        create_info.descriptor_types |= DescriptorType::DESCRIPTOR_TYPE_COLOR_ATTACHMENT;
        create_info.descriptor_types |= DescriptorType::DESCRIPTOR_TYPE_RW_TEXTURE |
                                        DescriptorType::DESCRIPTOR_TYPE_TEXTURE; // each rt can be rwtexture and texture
        create_info.texture_format = TextureFormat::TEXTURE_FORMAT_DUMMY_COLOR;
    } else if (render_target_create_info.rt_type == RenderTargetType::DEPTH_STENCIL) {
        create_info.descriptor_types |= DescriptorType::DESCRIPTOR_TYPE_DEPTH_STENCIL_ATTACHMENT;
        create_info.descriptor_types |= DescriptorType::DESCRIPTOR_TYPE_TEXTURE; // each rt can be rwtexture and texture
    }
    create_info.texture_format = render_target_create_info.rt_format;
    create_info.texture_type = TextureType::TEXTURE_TYPE_2D;
    create_info.initial_state = ResourceState::RESOURCE_STATE_RENDER_TARGET;
    create_info.width = render_target_create_info.width;
    create_info.height = render_target_create_info.height;
    create_info.depth = 1;
    m_texture = new VulkanTexture(m_context, create_info);
}

Horizon::RHI::VulkanRenderTarget::~VulkanRenderTarget() noexcept {
    delete m_texture;
    m_texture = nullptr;
}
