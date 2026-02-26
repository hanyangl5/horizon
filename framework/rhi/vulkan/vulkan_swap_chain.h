#pragma once

#include <core/definations.h>
#include <rhi/swap_chain.h>
#include <rhi/vulkan/vulkan_utils.h>
namespace Horizon::Backend
{

class VulkanSwapChain : public SwapChain
{
  public:
    VulkanSwapChain(const VulkanRendererContext &context, const SwapChainCreateInfo &SwapChain_create_info,
                    Window *window) noexcept;
    virtual ~VulkanSwapChain() noexcept;
    bool Resize(u32 new_width, u32 new_height) noexcept;
    VulkanSwapChain(const VulkanSwapChain &rhs) noexcept = delete;
    VulkanSwapChain &operator=(const VulkanSwapChain &rhs) noexcept = delete;
    VulkanSwapChain(VulkanSwapChain &&rhs) noexcept = delete;
    VulkanSwapChain &operator=(VulkanSwapChain &&rhs) noexcept = delete;

  public:
    const VulkanRendererContext &m_context{};
    VkSurfaceKHR surface{};
    VkSurfaceFormatKHR optimal_surface_format{};
    VkSwapchainKHR swap_chain{};
    std::vector<VkImage> swap_chain_images{};
    std::vector<VkImageView> swap_chain_image_views{};

  private:
    void CreateSwapChainImagesAndViews(u32 new_width, u32 new_height, VkSwapchainKHR old_swap_chain) noexcept;
};
} // namespace Horizon::Backend
