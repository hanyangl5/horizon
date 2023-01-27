#pragma once

#include <runtime/core/utils/definations.h>
#include <runtime/function/rhi/vulkan/vulkan_utils.h>
#include <runtime/function/rhi/swap_chain.h>

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
  public:
    const VulkanRendererContext &m_context{};
    VkSurfaceKHR surface{};
    VkSurfaceFormatKHR optimal_surface_format{};
    VkSwapchainKHR swap_chain{};
    Container::Array<VkImage> swap_chain_images{};
    Container::Array<VkImageView> swap_chain_image_views{};
};
} // namespace Horizon::Backend
