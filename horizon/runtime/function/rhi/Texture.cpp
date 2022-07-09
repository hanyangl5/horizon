#include <runtime/function/rhi/Texture.h>

namespace Horizon::RHI {

Texture::Texture(const TextureCreateInfo &texture_create_info) noexcept
    : m_descriptor_type(texture_create_info.descriptor_type),
      m_type(texture_create_info.texture_type),
      m_format(texture_create_info.texture_format),
      m_state(texture_create_info.initial_state),
      m_width(texture_create_info.width), m_height(texture_create_info.height),
      m_depth(texture_create_info.depth) {}

} // namespace Horizon::RHI
