#include "metal_texture.h"

namespace Horizon::Backend
{

using namespace MetalUtils;

MetalTexture::MetalTexture(const TextureCreateInfo &texture_create_info, MTL::Texture *texture, bool owns_texture,
                           bool is_swap_chain_texture) noexcept
    : Texture(texture_create_info), m_texture(texture), m_owns_texture(owns_texture),
      m_is_swap_chain_texture(is_swap_chain_texture)
{
    if (m_texture != nil && !m_debug_name.empty())
    {
        m_texture->setLabel(ToNSString(m_debug_name));
    }
}

MetalTexture::~MetalTexture() noexcept
{
    if (m_owns_texture && m_texture != nil)
    {
        m_texture->release();
    }
    m_texture = nil;
}

void MetalTexture::SetNativeTexture(MTL::Texture *texture) noexcept
{
    m_texture = texture;
}

} // namespace Horizon::Backend
