#pragma once

#include <runtime/core/utils/definations.h>
#include <runtime/function/rhi/RHIUtils.h>

namespace Horizon::Backend {

class Texture {
  public:
    Texture(const TextureCreateInfo &texture_create_info) noexcept;
    virtual ~Texture() noexcept = default;
    Texture(const Texture &rhs) noexcept = delete;
    Texture &operator=(const Texture &rhs) noexcept = delete;
    Texture(Texture &&rhs) noexcept = delete;
    Texture &operator=(Texture &&rhs) noexcept = delete;
    // virtual void *GetBufferPointer() noexcept = 0;
  public:
    DescriptorTypes m_descriptor_types;
    ResourceState m_state{};
    const TextureType m_type{};
    const TextureFormat m_format{};

    const u32 m_width{}, m_height{}, m_depth{};
    u32 mip_map_level{};
    const u32 m_byte_per_pixel{};

  protected:
};
} // namespace Horizon::Backend
