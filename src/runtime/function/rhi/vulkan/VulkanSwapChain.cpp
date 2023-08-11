#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#endif

#include <vulkan/vulkan.hpp>

#include "VulkanRenderTarget.h"
#include "VulkanSemaphore.h"
#include "VulkanSwapChain.h"

Horizon::Backend::VulkanSwapChain::VulkanSwapChain(const VulkanRendererContext &context,
                                                   const SwapChainCreateInfo &swap_chain_create_info,
                                                   Window *window) noexcept
    : SwapChain(swap_chain_create_info, window), m_context(context) {
    // create window surface
    VkWin32SurfaceCreateInfoKHR surface_create_info{};
    surface_create_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surface_create_info.hinstance = GetModuleHandle(nullptr);
    ;
    surface_create_info.hwnd = window->GetWin32Window();
    CHECK_VK_RESULT(vkCreateWin32SurfaceKHR(m_context.instance, &surface_create_info, nullptr, &surface));

    u32 surface_format_count = 0;
    // Get surface formats count
    CHECK_VK_RESULT(
        vkGetPhysicalDeviceSurfaceFormatsKHR(m_context.active_gpu, surface, &surface_format_count, nullptr));
    auto stack_memory = Memory::GetStackMemoryResource(32);
    Container::Array<VkSurfaceFormatKHR> surface_formats(surface_format_count, &stack_memory);
    CHECK_VK_RESULT(vkGetPhysicalDeviceSurfaceFormatsKHR(m_context.active_gpu, surface, &surface_format_count,
                                                         surface_formats.data()));

    optimal_surface_format = surface_formats.back();

    // create swap chain

    VkSwapchainCreateInfoKHR vk_swap_chain_create_info{};
    vk_swap_chain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    vk_swap_chain_create_info.surface = surface;
    vk_swap_chain_create_info.minImageCount = m_back_buffer_count;
    vk_swap_chain_create_info.imageFormat = VK_FORMAT_R8G8B8A8_UNORM;              // optimal_surface_format.format;
    vk_swap_chain_create_info.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR; // optimal_surface_format.colorSpace;
    vk_swap_chain_create_info.imageExtent = {window->GetWidth(), window->GetHeight()};
    vk_swap_chain_create_info.imageArrayLayers = 1;
    vk_swap_chain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                                           VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                           VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    vk_swap_chain_create_info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR; // rotatioin/flip
    vk_swap_chain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    vk_swap_chain_create_info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    vk_swap_chain_create_info.clipped = VK_TRUE;
    vk_swap_chain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vk_swap_chain_create_info.queueFamilyIndexCount = 0;

    CHECK_VK_RESULT(vkCreateSwapchainKHR(m_context.device, &vk_swap_chain_create_info, nullptr, &swap_chain));

    // u32 image_count = 0;
    // vkGetSwapchainImagesKHR(m_context.device, m_context.swap_chain,
    // &image_count, nullptr);  // Get images
    swap_chain_images.resize(m_back_buffer_count);
    vkGetSwapchainImagesKHR(m_context.device, swap_chain, &m_back_buffer_count,
                            swap_chain_images.data()); // Get images

    swap_chain_image_views.resize(m_back_buffer_count);

    VkImageViewCreateInfo image_view_create_info{};
    image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    image_view_create_info.format = VK_FORMAT_R8G8B8A8_UNORM;
    image_view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_view_create_info.subresourceRange.baseMipLevel = 0;
    image_view_create_info.subresourceRange.levelCount = 1;
    image_view_create_info.subresourceRange.baseArrayLayer = 0;
    image_view_create_info.subresourceRange.layerCount = 1;

    for (u32 i = 0; i < swap_chain_image_views.size(); i++) {
        image_view_create_info.image = swap_chain_images[i];
        CHECK_VK_RESULT(
            vkCreateImageView(m_context.device, &image_view_create_info, nullptr, &swap_chain_image_views[i]));
        render_targets.push_back(
            new VulkanRenderTarget(m_context, RenderTargetCreateInfo{RenderTargetFormat::TEXTURE_FORMAT_DUMMY_COLOR,
                                                                     RenderTargetType::UNDEFINED, width, height}));
        auto tx = reinterpret_cast<VulkanTexture *>(render_targets[i]->GetTexture());
        tx->m_image_view = swap_chain_image_views[i];
        tx->m_image = swap_chain_images[i];
    }
}

Horizon::Backend::VulkanSwapChain::~VulkanSwapChain() noexcept {

    for (u32 i = 0; i < swap_chain_images.size(); i++) {
        vkDestroyImageView(m_context.device, swap_chain_image_views[i], nullptr);
        // vkDestroyImage(m_vulkan.device, m_vulkan.swap_chain_images[i],
        // nullptr);
    }
    vkDestroySwapchainKHR(m_context.device, swap_chain, nullptr);
    vkDestroySurfaceKHR(m_context.instance, surface, nullptr);
}

void Horizon::Backend::VulkanSwapChain::AcquireNextFrame(SwapChainSemaphoreContext *recycled_sempahores) noexcept {}
