#pragma once

#include <runtime/function/rhi/rhi_utils.h>
#include <runtime/function/rhi/Texture.h>

namespace Horizon::Backend {

class RenderTarget {
  public:
    RenderTarget(const RenderTargetCreateInfo &render_target_create_info) noexcept {};
    virtual ~RenderTarget() noexcept { delete m_texture; };

    RenderTarget(const RenderTarget &rhs) noexcept = delete;
    RenderTarget &operator=(const RenderTarget &rhs) noexcept = delete;
    RenderTarget(RenderTarget &&rhs) noexcept = delete;
    RenderTarget &operator=(RenderTarget &&rhs) noexcept = delete;

    Texture *GetTexture() noexcept { return m_texture; }

  public:
    Texture *m_texture{};
};
} // namespace Horizon::Backend
