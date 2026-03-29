#include "metal_render_target.h"

namespace Horizon::Backend
{

MetalRenderTarget::MetalRenderTarget(Texture *texture) noexcept
{
    m_texture = texture;
}

} // namespace Horizon::Backend
