#pragma once

#include <runtime/core/utils/Definations.h>
#include <runtime/function/rhi/vulkan/VulkanUtils.h>
#include <runtime/function/rhi/SwapChain.h>
namespace Horizon::Backend {

class VulkanSwapChain : public SwapChain{
  public:
    VulkanSwapChain(const VulkanRendererContext &context, const SwapChainCreateInfo &SwapChain_create_info,
                    Window *window) noexcept;
    virtual ~VulkanSwapChain() noexcept;
    VulkanSwapChain(const VulkanSwapChain &rhs) noexcept = delete;
    VulkanSwapChain &operator=(const VulkanSwapChain &rhs) noexcept = delete;
    VulkanSwapChain(VulkanSwapChain &&rhs) noexcept = delete;
    VulkanSwapChain &operator=(VulkanSwapChain &&rhs) noexcept = delete;

    void AcquireNextFrame(SwapChainSemaphoreContext *recycled_sempahores) noexcept override;

  public:
    const VulkanRendererContext &m_context{};
    VkSurfaceKHR surface{};
    VkSurfaceFormatKHR optimal_surface_format{};
    VkSwapchainKHR swap_chain{};
    std::vector<VkImage> swap_chain_images{};
    std::vector<VkImageView> swap_chain_image_views{};
};
} // namespace Horizon::Backend
