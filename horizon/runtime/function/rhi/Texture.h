#pragma once

#include <runtime/core/utils/Definations.h>
#include <runtime/function/rhi/RHIUtils.h>

namespace Horizon::RHI {

struct TextureData {
    void *data;
    u32 row_length;
    u32 height;
};
class Texture {
  public:
    Texture(const TextureCreateInfo &texture_create_info) noexcept;
    virtual ~Texture() noexcept = default;
    // virtual void *GetBufferPointer() noexcept = 0;
  public:
    const DescriptorType m_descriptor_type;
    ResourceState m_state{};
    const TextureType m_type{};
    const TextureFormat m_format{};

    const u32 m_width{}, m_height{}, m_depth{};

  protected:
};
} // namespace Horizon::RHI
