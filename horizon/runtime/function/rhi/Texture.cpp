#include <runtime/function/rhi/Texture.h>

namespace Horizon::Backend {

Texture::Texture(const TextureCreateInfo &texture_create_info) noexcept
    : m_descriptor_types(texture_create_info.descriptor_types), m_type(texture_create_info.texture_type),
      m_format(texture_create_info.texture_format), m_state(texture_create_info.initial_state),
      m_width(texture_create_info.width), m_height(texture_create_info.height), m_depth(texture_create_info.depth),
      m_byte_per_pixel(GetBytesFromTextureFormat(m_format)) {

    mip_map_level =
        texture_create_info.enanble_mipmap == true
            ? std::min(MAX_MIP_LEVEL, static_cast<uint32_t>(std::floor(std::log2(std::max(m_width, m_height))))) + 1
            : 1;
}

} // namespace Horizon::Backend
