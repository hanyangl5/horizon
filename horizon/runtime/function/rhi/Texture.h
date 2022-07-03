#pragma once

#include <runtime/core/utils/Definations.h>
#include <runtime/function/rhi/RHIUtils.h>

namespace Horizon::RHI {
class Texture {
  public:
    Texture(const TextureCreateInfo &texture_create_info) noexcept;
    virtual ~Texture() noexcept = default;

  protected:
    TextureType m_type{};
    TextureFormat m_format{};
    u32 m_usage{};
    u32 m_width{}, m_height{}, m_depth{};
};
} // namespace Horizon::RHI
