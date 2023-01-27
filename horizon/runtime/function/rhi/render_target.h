#pragma once

#include <runtime/function/rhi/rhi_utils.h>
#include <runtime/function/rhi/texture.h>

namespace Horizon::Backend {

class RenderTarget {
  public:
    RenderTarget(const RenderTargetCreateInfo &render_target_create_info) noexcept {
        rt_format = render_target_create_info.rt_format;
        width = render_target_create_info.height;
        height = render_target_create_info.height;
        rt_type = render_target_create_info.rt_type;
    };
    virtual ~RenderTarget() noexcept {};

    RenderTarget(const RenderTarget &rhs) noexcept = delete;
    RenderTarget &operator=(const RenderTarget &rhs) noexcept = delete;
    RenderTarget(RenderTarget &&rhs) noexcept = delete;
    RenderTarget &operator=(RenderTarget &&rhs) noexcept = delete;

    Texture *GetTexture() noexcept { return m_texture; }

  protected:
    RenderTargetFormat rt_format;
    RenderTargetType rt_type;
    u32 width, height;
    Texture *m_texture{};
};
} // namespace Horizon::Backend
