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
	//CreateImageViews();
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
	return u32(images.size());
}

VkFormat SwapChain::getImageFormat() const
{
	return imageFormat;
}

void SwapChain::recreate(VkExtent2D newExtent)
{
	cleanup();

	create();

	//	CreateImageViews();
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

	VkSwapchainCreateInfoKHR createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = mSurface->get();
	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = mExtent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT; //  the image can be used as the source of a transfer command.
	createInfo.preTransform = surfaceCapabilities.currentTransform; // rotatioin/flip
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;

	// handle swap chain images that will be used across multiple queue families
	if (indices.getGraphics() != indices.getPresent())
	{
		std::vector<u32> queueFamilyindices{ indices.getGraphics(),indices.getPresent() };
		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;  // An image is owned by one queue family at a time and ownership must be explicitly transferred before using it in another queue family. This option offers the best performance.
		createInfo.queueFamilyIndexCount = static_cast<u32>(queueFamilyindices.size());
		createInfo.pQueueFamilyIndices = queueFamilyindices.data();
	}
	else {
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; // mages can be used across multiple queue families without explicit ownership transfers.
		createInfo.queueFamilyIndexCount = 0;
		createInfo.pQueueFamilyIndices = nullptr;
	}

	printVkError(vkCreateSwapchainKHR(mDevice->get(), &createInfo, nullptr, &mSwapChain), "create swap chain");


	//SaveImages(minImageCount);
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

//void SwapChain::saveImages(u32 imageCount)
//{
//	// real count of images can be greater than requested
//	vkGetSwapchainImagesKHR(device->Get(), swap_chain, &imageCount, nullptr);  // get count
//	images.resize(imageCount);
//	vkGetSwapchainImagesKHR(device->Get(), swap_chain, &imageCount, images.data());  // get images
//}
//
//void SwapChain::createImageViews()
//{
//	imageViews.resize(GetImageCount());
//
//	for (u32 i = 0; i < GetImageCount(); i++)
//	{
//
//		//Image image(device, images[i], imageFormat, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT);
//
//		VkImageViewCreateInfo create_info{};
//		create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
//		create_info.image = images[i];
//		create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
//		create_info.format = imageFormat;
//		create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//		create_info.subresourceRange.baseMipLevel = 0;
//		create_info.subresourceRange.levelCount = 1;
//		create_info.subresourceRange.baseArrayLayer = 0;
//		create_info.subresourceRange.layerCount = 1;
//
//		VkResult res = vkCreateImageView(device->Get(), &create_info, nullptr, &imageViews[i]);
//		Log::Assert(res, "failed to create image view");
//	}
//}

void SwapChain::cleanup()
{
	//for (auto imageView : imageViews)
	//{
	//	vkDestroyImageView(mDevice, imageView, nullptr);
	//}

	vkDestroySwapchainKHR(mDevice->get(), mSwapChain, nullptr);
}

