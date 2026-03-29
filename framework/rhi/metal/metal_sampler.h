#pragma once

#include <rhi/metal/metal_utils.h>

namespace Horizon::Backend
{

class MetalSampler final : public Sampler
{
  public:
    explicit MetalSampler(MTL::SamplerState *sampler_state) noexcept;
    ~MetalSampler() noexcept override;
    MetalSampler(const MetalSampler &rhs) noexcept = delete;
    MetalSampler &operator=(const MetalSampler &rhs) noexcept = delete;
    MetalSampler(MetalSampler &&rhs) noexcept = delete;
    MetalSampler &operator=(MetalSampler &&rhs) noexcept = delete;

    MTL::SamplerState *m_sampler_state{};
};

} // namespace Horizon::Backend
