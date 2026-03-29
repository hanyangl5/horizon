#pragma once

#include <rhi/metal/metal_utils.h>

namespace Horizon::Backend
{

class MetalTexture final : public Texture
{
  public:
    MetalTexture(const TextureCreateInfo &texture_create_info, MTL::Texture *texture, bool owns_texture,
                 bool is_swap_chain_texture = false) noexcept;
    ~MetalTexture() noexcept override;
    MetalTexture(const MetalTexture &rhs) noexcept = delete;
    MetalTexture &operator=(const MetalTexture &rhs) noexcept = delete;
    MetalTexture(MetalTexture &&rhs) noexcept = delete;
    MetalTexture &operator=(MetalTexture &&rhs) noexcept = delete;

    void SetNativeTexture(MTL::Texture *texture) noexcept;

    MTL::Texture *m_texture{};
    bool m_owns_texture{true};
    bool m_is_swap_chain_texture{false};
};

} // namespace Horizon::Backend
