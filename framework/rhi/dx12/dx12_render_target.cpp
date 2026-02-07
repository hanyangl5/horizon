#include "dx12_render_target.h"
#include "dx12_texture.h"
#include <core/log.h>
#include <core/memory.h>

namespace Horizon::Backend
{

DX12RenderTarget::DX12RenderTarget(const DX12RendererContext &context,
                                   const RenderTargetCreateInfo &render_target_create_info) noexcept
    : RenderTarget(), m_context(context)
{
    // Create texture for render target
    TextureCreateInfo texture_create_info{};
    texture_create_info.width = render_target_create_info.width;
    texture_create_info.height = render_target_create_info.height;
    texture_create_info.texture_type = TextureType::TEXTURE_TYPE_2D;
    texture_create_info.texture_format = render_target_create_info.rt_format;
    texture_create_info.descriptor_types = (render_target_create_info.rt_type == RenderTargetType::COLOR)
                                               ? DESCRIPTOR_TYPE_RENDER_TARGET_MIP_SLICES
                                               : DESCRIPTOR_TYPE_DEPTH_STENCIL_ATTACHMENT;
    texture_create_info.initial_state = (render_target_create_info.rt_type == RenderTargetType::COLOR)
                                            ? RESOURCE_STATE_RENDER_TARGET
                                            : RESOURCE_STATE_DEPTH_WRITE;

    m_texture = Memory::Alloc<DX12Texture>(context, texture_create_info);

    // TODO: Create RTV or DSV descriptor
    // This requires a descriptor heap which will be implemented later
}

DX12RenderTarget::~DX12RenderTarget() noexcept
{
    // Base class will delete m_texture
}

} // namespace Horizon::Backend
