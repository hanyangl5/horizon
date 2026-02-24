#pragma once

#include <core/memory.h>
#include <rhi/enums.h>
#include <rhi/texture.h>

namespace Horizon::Backend
{

class RenderTarget
{
  public:
    RenderTarget() noexcept {};
    virtual ~RenderTarget() noexcept
    {
        Memory::Free(m_texture);
    };

    RenderTarget(const RenderTarget &rhs) noexcept = delete;
    RenderTarget &operator=(const RenderTarget &rhs) noexcept = delete;
    RenderTarget(RenderTarget &&rhs) noexcept = delete;
    RenderTarget &operator=(RenderTarget &&rhs) noexcept = delete;

    Texture *GetTexture() noexcept
    {
        return m_texture;
    }

  public:
    Texture *m_texture{};
};
} // namespace Horizon::Backend
