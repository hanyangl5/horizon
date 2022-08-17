#pragma once

#include <runtime/core/utils/Definations.h>
#include <runtime/function/rhi/RHIUtils.h>

namespace Horizon::RHI {

class RenderTarget {
  public:
    RenderTarget(
        const RenderTargetCreateInfo &render_target_create_info) noexcept;
    virtual ~RenderTarget() noexcept = default;

  public:
    const DescriptorType m_descriptor_type;
    ResourceState m_state{};
    const TextureType m_type{};
    const TextureFormat m_format{};

    const u32 m_width{}, m_height{}, m_depth{};
};
} // namespace Horizon::RHI
