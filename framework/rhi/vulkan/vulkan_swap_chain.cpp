#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#endif

#ifdef __ANDROID__
#define VK_USE_PLATFORM_ANDROID_KHR
#include <android/native_window.h>
#endif

#include <volk.h>

#include "vulkan_render_target.h"
#include "vulkan_semaphore.h"
#include "vulkan_swap_chain.h"

Horizon::Backend::VulkanSwapChain::VulkanSwapChain(const VulkanRendererContext &context,
                                                   const SwapChainCreateInfo &swap_chain_create_info,
                                                   Window *window) noexcept
    : SwapChain(swap_chain_create_info, window), m_context(context)
{
    // create window surface
#ifdef __ANDROID__
    // Create Android surface
    if (window == nullptr)
    {
        LOG_ERROR("Cannot create Android Vulkan surface: window is null");
        return;
    }

    ANativeWindow *native_window = window->GetNativeWindow();
    if (native_window == nullptr)
    {
        LOG_ERROR("Cannot create Android Vulkan surface: native window is null");
        return;
    }

    auto create_android_surface = reinterpret_cast<PFN_vkCreateAndroidSurfaceKHR>(
        vkGetInstanceProcAddr(m_context.instance, "vkCreateAndroidSurfaceKHR"));
    if (create_android_surface == nullptr)
    {
        LOG_ERROR("vkCreateAndroidSurfaceKHR is not available on this instance");
        return;
    }

    VkAndroidSurfaceCreateInfoKHR surface_create_info{};
    surface_create_info.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    surface_create_info.pNext = nullptr;
    surface_create_info.flags = 0;
    surface_create_info.window = native_window;

    CHECK_VK_RESULT(create_android_surface(m_context.instance, &surface_create_info, nullptr, &surface));
#else
    // Use GLFW to create surface
    CHECK_VK_RESULT(glfwCreateWindowSurface(m_context.instance, window->GetWindow(), nullptr, &surface));
#endif
    u32 surface_format_count = 0;
    // Get surface formats count
    CHECK_VK_RESULT(
        vkGetPhysicalDeviceSurfaceFormatsKHR(m_context.active_gpu, surface, &surface_format_count, nullptr));

    std::vector<VkSurfaceFormatKHR> surface_formats(surface_format_count);
    CHECK_VK_RESULT(vkGetPhysicalDeviceSurfaceFormatsKHR(m_context.active_gpu, surface, &surface_format_count,
                                                         surface_formats.data()));

    if (surface_formats.empty())
    {
        LOG_ERROR("No Vulkan surface formats available");
        return;
    }

    optimal_surface_format = surface_formats[0];
    for (const auto &format : surface_formats)
    {
        if (format.format == VK_FORMAT_R8G8B8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            optimal_surface_format = format;
            break;
        }
    }
    CreateSwapChainImagesAndViews(window->GetWidth(), window->GetHeight(), VK_NULL_HANDLE);
}

Horizon::Backend::VulkanSwapChain::~VulkanSwapChain() noexcept
{

    for (u32 i = 0; i < swap_chain_image_views.size(); i++)
    {
        vkDestroyImageView(m_context.device, swap_chain_image_views[i], nullptr);
    }
    vkDestroySwapchainKHR(m_context.device, swap_chain, nullptr);
    vkDestroySurfaceKHR(m_context.instance, surface, nullptr);
}

bool Horizon::Backend::VulkanSwapChain::Resize(u32 new_width, u32 new_height, bool force_recreate) noexcept
{
    if (new_width == 0 || new_height == 0)
    {
        return false;
    }
    if (!force_recreate && new_width == width && new_height == height)
    {
        return true;
    }

    vkDeviceWaitIdle(m_context.device);

    for (u32 i = 0; i < swap_chain_image_views.size(); i++)
    {
        vkDestroyImageView(m_context.device, swap_chain_image_views[i], nullptr);
    }
    swap_chain_image_views.clear();

    VkSwapchainKHR old_swap_chain = swap_chain;
    CreateSwapChainImagesAndViews(new_width, new_height, old_swap_chain);
    return true;
}

void Horizon::Backend::VulkanSwapChain::SetVSyncEnabled(bool enabled) noexcept
{
    if (m_enable_vsync == enabled)
    {
        return;
    }

    m_enable_vsync = enabled;
    Resize(width, height, true);
}

void Horizon::Backend::VulkanSwapChain::CreateSwapChainImagesAndViews(u32 new_width, u32 new_height,
                                                                      VkSwapchainKHR old_swap_chain) noexcept
{
    width = new_width;
    height = new_height;

    VkSwapchainCreateInfoKHR vk_swap_chain_create_info{};
    vk_swap_chain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    vk_swap_chain_create_info.surface = surface;
    vk_swap_chain_create_info.minImageCount = m_back_buffer_count;
    vk_swap_chain_create_info.imageFormat = optimal_surface_format.format;
    vk_swap_chain_create_info.imageColorSpace = optimal_surface_format.colorSpace;
    vk_swap_chain_create_info.imageExtent = {new_width, new_height};
    vk_swap_chain_create_info.imageArrayLayers = 1;
    vk_swap_chain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                                           VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                           VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    vk_swap_chain_create_info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR; // rotatioin/flip
    vk_swap_chain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    u32 present_mode_count = 0;
    CHECK_VK_RESULT(
        vkGetPhysicalDeviceSurfacePresentModesKHR(m_context.active_gpu, surface, &present_mode_count, nullptr));
    std::vector<VkPresentModeKHR> present_modes(present_mode_count);
    CHECK_VK_RESULT(vkGetPhysicalDeviceSurfacePresentModesKHR(m_context.active_gpu, surface, &present_mode_count,
                                                              present_modes.data()));
    vk_swap_chain_create_info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    if (!m_enable_vsync)
    {
        for (const auto mode : present_modes)
        {
            if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
            {
                vk_swap_chain_create_info.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
                break;
            }
            if (mode == VK_PRESENT_MODE_IMMEDIATE_KHR)
            {
                vk_swap_chain_create_info.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
            }
        }
    }
    vk_swap_chain_create_info.clipped = VK_TRUE;
    vk_swap_chain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vk_swap_chain_create_info.queueFamilyIndexCount = 0;
    vk_swap_chain_create_info.oldSwapchain = old_swap_chain;

    CHECK_VK_RESULT(vkCreateSwapchainKHR(m_context.device, &vk_swap_chain_create_info, nullptr, &swap_chain));

    if (old_swap_chain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(m_context.device, old_swap_chain, nullptr);
    }

    u32 swap_chain_image_count = 0;
    CHECK_VK_RESULT(vkGetSwapchainImagesKHR(m_context.device, swap_chain, &swap_chain_image_count, nullptr));
    if (swap_chain_image_count == 0)
    {
        LOG_ERROR("Swapchain returned zero images");
        return;
    }
    LOG_INFO("Swapchain returned {} images", swap_chain_image_count);

    swap_chain_images.resize(swap_chain_image_count);
    CHECK_VK_RESULT(
        vkGetSwapchainImagesKHR(m_context.device, swap_chain, &swap_chain_image_count, swap_chain_images.data()));

    swap_chain_image_views.resize(swap_chain_image_count);

    VkImageViewCreateInfo image_view_create_info{};
    image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    image_view_create_info.format = optimal_surface_format.format;
    image_view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_view_create_info.subresourceRange.baseMipLevel = 0;
    image_view_create_info.subresourceRange.levelCount = 1;
    image_view_create_info.subresourceRange.baseArrayLayer = 0;
    image_view_create_info.subresourceRange.layerCount = 1;

    for (u32 i = 0; i < swap_chain_image_views.size(); i++)
    {
        image_view_create_info.image = swap_chain_images[i];
        CHECK_VK_RESULT(
            vkCreateImageView(m_context.device, &image_view_create_info, nullptr, &swap_chain_image_views[i]));

        if (render_targets.size() <= i)
        {
            render_targets.push_back(
                new VulkanRenderTarget(m_context, RenderTargetCreateInfo{RenderTargetFormat::TEXTURE_FORMAT_DUMMY_COLOR,
                                                                         RenderTargetType::UNDEFINED, width, height}));
        }
        auto tx = reinterpret_cast<VulkanTexture *>(render_targets[i]->GetTexture());
        tx->m_image_view = swap_chain_image_views[i];
        tx->m_image = swap_chain_images[i];
    }

    if (m_back_buffer_count > 0)
    {
        current_frame_index = current_frame_index % m_back_buffer_count;
        image_index = image_index % m_back_buffer_count;
    }
}
