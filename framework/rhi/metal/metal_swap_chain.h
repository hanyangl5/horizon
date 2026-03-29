#pragma once

#include <rhi/metal/metal_utils.h>
#include <rhi/swap_chain.h>

namespace Horizon::Backend
{

class MetalSwapChain final : public SwapChain
{
  public:
    MetalSwapChain(const SwapChainCreateInfo &swap_chain_create_info, Window *window) noexcept;
    ~MetalSwapChain() noexcept override;
    MetalSwapChain(const MetalSwapChain &rhs) noexcept = delete;
    MetalSwapChain &operator=(const MetalSwapChain &rhs) noexcept = delete;
    MetalSwapChain(MetalSwapChain &&rhs) noexcept = delete;
    MetalSwapChain &operator=(MetalSwapChain &&rhs) noexcept = delete;

    bool Initialize(MTL::Device *device, Window *window) noexcept;
    void SetVSyncEnabled(bool enabled) noexcept override;
    void UpdateDrawableSize(Window *window) noexcept;

    CA::MetalLayer *m_layer{};
    CA::MetalDrawable *m_current_drawable{};
};

} // namespace Horizon::Backend
