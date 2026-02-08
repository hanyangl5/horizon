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

    // Note: RTV/DSV descriptors should be created externally using descriptor heap allocator
}

DX12RenderTarget::DX12RenderTarget(const DX12RendererContext &context, ComPtr<ID3D12Resource> back_buffer_resource,
                                   D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle) noexcept
    : RenderTarget(), m_context(context), m_back_buffer_resource(back_buffer_resource), m_rtv_handle(rtv_handle)
{
    // This constructor is for swap chain back buffers
    // The texture is not created as the back buffer resource is managed by the swap chain
}

DX12RenderTarget::~DX12RenderTarget() noexcept
{
    // Base class will delete m_texture
}

} // namespace Horizon::Backend
