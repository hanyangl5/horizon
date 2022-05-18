#include "SwapChain.h"

#include <runtime/core/log/Log.h>

#include "Device.h"
#include "QueueFamilyIndices.h"
#include "SurfaceSupportDetails.h"
#include <runtime/function/window/Window.h>

namespace Horizon {
	SwapChain::SwapChain(RenderContext& render_context, std::shared_ptr<Device> device, std::shared_ptr<Surface> surface) :m_render_context(render_context), m_device(device), m_surface(surface)
	{
		createSwapChain();

		createImageViews();
	}

	SwapChain::~SwapChain()
	{
		cleanup();
	}

	VkSwapchainKHR SwapChain::Get() const noexcept
	{
		return m_swap_chain;
	}

	std::vector<VkImageView> SwapChain::getImageViews() const noexcept
	{
		return imageViews;
	}

	VkImageView SwapChain::getImageView(u32 i) const noexcept {
		return imageViews[i];
	}


	VkFormat SwapChain::getImageFormat() const noexcept
	{
		return mImageFormat;
	}

	void SwapChain::recreate(VkExtent2D newExtent)
	{
		cleanup();

		createSwapChain();

		createImageViews();
	}

	// private:

	void SwapChain::createSwapChain()
	{
		// Get necessary swapchain properties
		SurfaceSupportDetails details(m_device->getPhysicalDevice(), m_surface->Get());
		QueueFamilyIndices indices = m_device->getQueueFamilyIndices();
		VkSurfaceFormatKHR surfaceFormat = chooseSurfaceFormat(details.getFormats());
		VkPresentModeKHR presentMode = choosePresentMode(details.getPresentModes());
		VkSurfaceCapabilitiesKHR surfaceCapabilities = details.getCapabilities();
		u32 imag_count = m_render_context.swap_chain_image_count; // how many images we would like to have in swap chain

		mImageFormat = surfaceFormat.format;

		VkSwapchainCreateInfoKHR swap_chain_create_info{};
		swap_chain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		swap_chain_create_info.surface = m_surface->Get();
		swap_chain_create_info.minImageCount = imag_count;
		swap_chain_create_info.imageFormat = mImageFormat;
		swap_chain_create_info.imageColorSpace = surfaceFormat.colorSpace;
		swap_chain_create_info.imageExtent = { m_render_context.width, m_render_context.height };
		swap_chain_create_info.imageArrayLayers = 1;
		swap_chain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT; //  the image can be used as the source of a transfer command.
		swap_chain_create_info.preTransform = surfaceCapabilities.currentTransform; // rotatioin/flip
		swap_chain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		swap_chain_create_info.presentMode = presentMode;
		swap_chain_create_info.clipped = VK_TRUE;

		// handle swap chain images that will be used across multiple queue families
		if (indices.getGraphics() != indices.getPresent())
		{
			std::vector<u32> queueFamilyindices{ indices.getGraphics(),indices.getPresent() };
			swap_chain_create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;  // An image is owned by one queue family at a time and ownership must be explicitly transferred before using it in another queue family. This option offers the best performance.
			swap_chain_create_info.queueFamilyIndexCount = static_cast<u32>(queueFamilyindices.size());
			swap_chain_create_info.pQueueFamilyIndices = queueFamilyindices.data();
		}
		else {
			swap_chain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; // mages can be used across multiple queue families without explicit ownership transfers.
			swap_chain_create_info.queueFamilyIndexCount = 0;
			swap_chain_create_info.pQueueFamilyIndices = nullptr;
		}

		CHECK_VK_RESULT(vkCreateSwapchainKHR(m_device->Get(), &swap_chain_create_info, nullptr, &m_swap_chain));

		vkGetSwapchainImagesKHR(m_device->Get(), m_swap_chain, &imag_count, nullptr);  // Get images
		images.resize(imag_count);
		vkGetSwapchainImagesKHR(m_device->Get(), m_swap_chain, &imag_count, images.data());  // Get images

	}

	VkSurfaceFormatKHR SwapChain::chooseSurfaceFormat(std::vector<VkSurfaceFormatKHR> availableFormats) const noexcept
	{
		// force predefined format
		VkSurfaceFormatKHR ret{};
		if (availableFormats.size() == 1 && availableFormats[0].format == VK_FORMAT_UNDEFINED)
		{
			return PREFERRED_PRESENT_FORMAT;
		}

		// try to found preferred format from available
		for (const auto& availableFormat : availableFormats)
		{
			if (availableFormat.format == PREFERRED_PRESENT_FORMAT.format &&
				availableFormat.colorSpace == PREFERRED_PRESENT_FORMAT.colorSpace)
			{
				return availableFormat;
			}
		}

		// first available format
		return availableFormats[0];
	}

	VkPresentModeKHR SwapChain::choosePresentMode(std::vector<VkPresentModeKHR> availablePresentModes) const noexcept
	{
		// simplest mode
		return VK_PRESENT_MODE_FIFO_KHR;
	}

	VkExtent2D SwapChain::chooseExtent(VkSurfaceCapabilitiesKHR capabilities)
	{
		// can choose any extent
		if (capabilities.currentExtent.width != (std::numeric_limits<u32>::max)())
		{
			return capabilities.currentExtent;
		}
		i32 width, height;
		glfwGetFramebufferSize(m_window->getWindow(), &width, &height);

		VkExtent2D actualExtent = {
			static_cast<u32>(width),
			static_cast<u32>(height)
		};

		// extent height and width: (minAvailable <= extent <= maxAvailable) && (extent <= actualExtent)
		actualExtent.width = (std::max)(
			capabilities.minImageExtent.width,
			(std::min)(capabilities.maxImageExtent.width, actualExtent.width));
		actualExtent.height = (std::max)(
			capabilities.minImageExtent.height,
			(std::min)(capabilities.maxImageExtent.height, actualExtent.height));

		return actualExtent;
	}

	u32 SwapChain::chooseMinImageCount(VkSurfaceCapabilitiesKHR capabilities)
	{
		u32 imag_count = capabilities.minImageCount + 1;
		if (capabilities.maxImageCount > 0 &&
			imag_count > capabilities.maxImageCount)
		{
			imag_count = capabilities.maxImageCount;// not exceed the maximum number of images
		}

		return imag_count;
	}



	void SwapChain::cleanup()
	{
		for (auto& imageView : imageViews)
		{
			vkDestroyImageView(m_device->Get(), imageView, nullptr);
		}
		vkDestroySwapchainKHR(m_device->Get(), m_swap_chain, nullptr);
	}

	void SwapChain::createImageViews()
	{
		imageViews.resize(m_render_context.swap_chain_image_count);

		for (u32 i = 0; i < m_render_context.swap_chain_image_count; i++)
		{
			//Image image(device, images[i], imageFormat, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT);

			VkImageViewCreateInfo image_view_create_info{};
			image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			image_view_create_info.image = images[i];
			image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
			image_view_create_info.format = mImageFormat;
			image_view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			image_view_create_info.subresourceRange.baseMipLevel = 0;
			image_view_create_info.subresourceRange.levelCount = 1;
			image_view_create_info.subresourceRange.baseArrayLayer = 0;
			image_view_create_info.subresourceRange.layerCount = 1;
			CHECK_VK_RESULT(vkCreateImageView(m_device->Get(), &image_view_create_info, nullptr, &imageViews[i]));
		}
	}
}
