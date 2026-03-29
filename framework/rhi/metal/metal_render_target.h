#pragma once

#include <rhi/render_target.h>

namespace Horizon::Backend
{

class MetalRenderTarget final : public RenderTarget
{
  public:
    explicit MetalRenderTarget(Texture *texture) noexcept;
    MetalRenderTarget(const MetalRenderTarget &rhs) noexcept = delete;
    MetalRenderTarget &operator=(const MetalRenderTarget &rhs) noexcept = delete;
    MetalRenderTarget(MetalRenderTarget &&rhs) noexcept = delete;
    MetalRenderTarget &operator=(MetalRenderTarget &&rhs) noexcept = delete;
};

} // namespace Horizon::Backend
