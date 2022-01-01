#include "SwapChain.h"
#include "Device.h"
#include "QueueFamilyIndices.h"
#include "SurfaceSupportDetails.h"
#include "utils.h"
#include "Window.h"

SwapChain::SwapChain(std::shared_ptr<Device> device, std::shared_ptr<Surface> surface, std::shared_ptr<Window> window)
{
	mDevice = device;
	mSurface = surface;
	mWindow = window;
	create();
	createImageViews();
}

SwapChain::~SwapChain()
{
	cleanup();
}

VkSwapchainKHR SwapChain::get() const
{
	return mSwapChain;
}


std::vector<VkImageView> SwapChain::getImageViews() const
{
	return imageViews;
}

VkImageView SwapChain::getImageView(u32 i)const {
	return imageViews[i];
}

VkExtent2D SwapChain::getExtent() const
{
	return mExtent;
}

u32 SwapChain::getImageCount() const
{
	return static_cast<u32>(images.size());
}

VkFormat SwapChain::getImageFormat() const
{
	return mImageFormat;
}

void SwapChain::recreate(VkExtent2D newExtent)
{
	cleanup();

	create();

	createImageViews();
}

// private:

void SwapChain::create()
{
	// get necessary swapchain properties
	SurfaceSupportDetails details(mDevice->getPhysicalDevice(), mSurface->get());
	QueueFamilyIndices indices = mDevice->getQueueFamilyIndices();
	VkSurfaceFormatKHR surfaceFormat = chooseSurfaceFormat(details.getFormats());
	VkPresentModeKHR presentMode = choosePresentMode(details.getPresentModes());
	VkSurfaceCapabilitiesKHR surfaceCapabilities = details.getCapabilities();
	u32 imageCount = chooseMinImageCount(details.getCapabilities()); // how many images we would like to have in swap chain
	mExtent = chooseExtent(details.getCapabilities());
	mImageFormat = surfaceFormat.format;

	VkSwapchainCreateInfoKHR swapChainCreateInfo{};
	swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapChainCreateInfo.surface = mSurface->get();
	swapChainCreateInfo.minImageCount = imageCount;
	swapChainCreateInfo.imageFormat = mImageFormat;
	swapChainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
	swapChainCreateInfo.imageExtent = mExtent;
	swapChainCreateInfo.imageArrayLayers = 1;
	swapChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT; //  the image can be used as the source of a transfer command.
	swapChainCreateInfo.preTransform = surfaceCapabilities.currentTransform; // rotatioin/flip
	swapChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapChainCreateInfo.presentMode = presentMode;
	swapChainCreateInfo.clipped = VK_TRUE;

	// handle swap chain images that will be used across multiple queue families
	if (indices.getGraphics() != indices.getPresent())
	{
		std::vector<u32> queueFamilyindices{ indices.getGraphics(),indices.getPresent() };
		swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;  // An image is owned by one queue family at a time and ownership must be explicitly transferred before using it in another queue family. This option offers the best performance.
		swapChainCreateInfo.queueFamilyIndexCount = static_cast<u32>(queueFamilyindices.size());
		swapChainCreateInfo.pQueueFamilyIndices = queueFamilyindices.data();
	}
	else {
		swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; // mages can be used across multiple queue families without explicit ownership transfers.
		swapChainCreateInfo.queueFamilyIndexCount = 0;
		swapChainCreateInfo.pQueueFamilyIndices = nullptr;
	}

	printVkError(vkCreateSwapchainKHR(mDevice->get(), &swapChainCreateInfo, nullptr, &mSwapChain), "create swap chain");

	// Retrieving the swap chain images
	saveImages(imageCount);

}

VkSurfaceFormatKHR SwapChain::chooseSurfaceFormat(std::vector<VkSurfaceFormatKHR> availableFormats) const
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

VkPresentModeKHR SwapChain::choosePresentMode(std::vector<VkPresentModeKHR> availablePresentModes) const
{
	for (const auto& availablePresentMode : availablePresentModes)
	{
		if (availablePresentMode == PREFERRED_PRESENT_MODE)
		{
			return availablePresentMode;
		}
	}

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
	int width, height;
	glfwGetFramebufferSize(mWindow->getWindow(), &width, &height);

	VkExtent2D actualExtent = {
	static_cast<uint32_t>(width),
	static_cast<uint32_t>(height)
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
	u32 imageCount = capabilities.minImageCount + 1;
	if (capabilities.maxImageCount > 0 &&
		imageCount > capabilities.maxImageCount)
	{
		imageCount = capabilities.maxImageCount;// not exceed the maximum number of images
	}

	return imageCount;
}

void SwapChain::saveImages(u32 imageCount)
{
	// real count of images can be greater than requested
	vkGetSwapchainImagesKHR(mDevice->get(), mSwapChain, &imageCount, nullptr);  // get count
	images.resize(imageCount);
	vkGetSwapchainImagesKHR(mDevice->get(), mSwapChain, &imageCount, images.data());  // get images
}

void SwapChain::createImageViews()
{
	imageViews.resize(getImageCount());

	for (u32 i = 0; i < getImageCount(); i++)
	{
		//Image image(device, images[i], imageFormat, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT);

		VkImageViewCreateInfo imageViewCreateInfo{};
		imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCreateInfo.image = images[i];
		imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewCreateInfo.format = mImageFormat;
		imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
		imageViewCreateInfo.subresourceRange.levelCount = 1;
		imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
		imageViewCreateInfo.subresourceRange.layerCount = 1;
		printVkError(vkCreateImageView(mDevice->get(), &imageViewCreateInfo, nullptr, &imageViews[i]),"create image views",logLevel::debug);
	}
}

void SwapChain::cleanup()
{
	for (auto imageView : imageViews)
	{
		vkDestroyImageView(mDevice->get(), imageView, nullptr);
	}

	vkDestroySwapchainKHR(mDevice->get(), mSwapChain, nullptr);
}

