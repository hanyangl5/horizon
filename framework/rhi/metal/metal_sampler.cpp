#include "metal_sampler.h"

namespace Horizon::Backend
{

MetalSampler::MetalSampler(MTL::SamplerState *sampler_state) noexcept : m_sampler_state(sampler_state)
{
}

MetalSampler::~MetalSampler() noexcept
{
    if (m_sampler_state != nil)
    {
        m_sampler_state->release();
        m_sampler_state = nil;
    }
}

} // namespace Horizon::Backend
