//#include "SwapChain.h"
//#include "Image.h"
//#include "SurfaceSupportDetails.h"
//SwapChain::SwapChain(std::shared_ptr<Device> device, VkSurfaceKHR surface, VkExtent2D surfaceExtent)
//{
//	mDevice = device->get();
//	mSurface = surface;
//	Create(surfaceExtent);
//	CreateImageViews();
//}
//
//SwapChain::~SwapChain()
//{
//	cleanup();
//}
//
//VkSwapchainKHR SwapChain::get() const
//{
//	return mSwapChain;
//}
//
//
//std::vector<VkImageView> SwapChain::getImageViews() const
//{
//	return imageViews;
//}
//
//VkImageView SwapChain::getImageView(u32 i)const {
//	return imageViews[i];
//}
//
//VkExtent2D SwapChain::getExtent() const
//{
//	return extent;
//}
//
//u32 SwapChain::getImageCount() const
//{
//	return u32(images.size());
//}
//
//VkFormat SwapChain::getImageFormat() const
//{
//	return imageFormat;
//}
//
//void SwapChain::recreate(VkExtent2D newExtent)
//{
//	Cleanup();
//
//	Create(newExtent);
//
//	CreateImageViews();
//}
//
//// private:
//
//void SwapChain::create(VkExtent2D surfaceExtent)
//{
//	// get necessary swapchain properties
//	const SurfaceSupportDetails details = device->GetSurfaceSupportDetails();
//	const VkSurfaceFormatKHR surfaceFormat = ChooseSurfaceFormat(details.getFormats());
//	const VkPresentModeKHR presentMode = ChoosePresentMode(details.getPresentModes());
//	const VkSurfaceCapabilitiesKHR surfaceCapabilities = details.getCapabilities();
//	const u32 minImageCount = ChooseMinImageCount(surfaceCapabilities);
//
//	imageFormat = surfaceFormat.format;
//	extent = ChooseExtent(details.getCapabilities(), surfaceExtent);
//	
//
//	VkSwapchainCreateInfoKHR createInfo{};
//	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
//	createInfo.pNext=nullptr;
//	createInfo.flags = 0;
//	createInfo.surface = surface;
//	createInfo.minImageCount = minImageCount;
//	createInfo.imageFormat=surfaceFormat.format;
//	createInfo.imageColorSpace = surfaceFormat.colorSpace;
//	createInfo.imageExtent=extent;
//	createInfo.imageArrayLayers = 1;
//	createInfo.imageUsage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
//	createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
//	createInfo.preTransform = surfaceCapabilities.currentTransform;
//	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
//	createInfo.presentMode = presentMode;
//	createInfo.clipped = VK_TRUE;
//
//	// concurrent sharing mode only when using different queue families
//	const QueueFamilyIndices queueFamilyIndices = device->GetQueueFamilyIndices();
//	std::vector<u32> indices{
//		queueFamilyIndices.GetGraphics(),
//		queueFamilyIndices.GetPresent()
//	};
//	if (indices[0] != indices[1])
//	{
//		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
//		createInfo.queueFamilyIndexCount = u32(indices.size());
//		createInfo.pQueueFamilyIndices = indices.data();
//	}
//
//	const VkResult result = vkCreateSwapchainKHR(device->Get(), &createInfo, nullptr, &swap_chain);
//	Log::Assert(result,"failed to create swap chain");
//
//	SaveImages(minImageCount);
//}
//
//VkSurfaceFormatKHR SwapChain::chooseSurfaceFormat(std::vector<VkSurfaceFormatKHR> availableFormats) const
//{
//	// force predefined format
//	VkSurfaceFormatKHR ret{};
//	if (availableFormats.size() == 1 && availableFormats[0].format == VK_FORMAT_UNDEFINED)
//	{
//		return PREFERRED_PRESENT_FORMAT;
//	}
//
//	// try to found preferred format from available
//	for (const auto& availableFormat : availableFormats)
//	{
//		if (availableFormat.format == PREFERRED_PRESENT_FORMAT.format &&
//			availableFormat.colorSpace == PREFERRED_PRESENT_FORMAT.colorSpace)
//		{
//			return availableFormat;
//		}
//	}
//
//	// first available format
//	return availableFormats[0];
//}
//
//VkPresentModeKHR SwapChain::choosePresentMode(std::vector<VkPresentModeKHR> availablePresentModes) const
//{
//	for (const auto& availablePresentMode : availablePresentModes)
//	{
//		if (availablePresentMode == PREFERRED_PRESENT_MODE)
//		{
//			return availablePresentMode;
//		}
//	}
//
//	// simplest mode
//	return VK_PRESENT_MODE_FIFO_KHR;
//}
//
//VkExtent2D SwapChain::chooseExtent(VkSurfaceCapabilitiesKHR capabilities, VkExtent2D actualExtent)
//{
//	// can choose any extent
//	if (capabilities.currentExtent.width != (std::numeric_limits<u32>::max)())
//	{
//		return capabilities.currentExtent;
//	}
//
//	// extent height and width: (minAvailable <= extent <= maxAvailable) && (extent <= actualExtent)
//	actualExtent.width = (std::max)(
//		capabilities.minImageExtent.width,
//		(std::min)(capabilities.maxImageExtent.width, actualExtent.width));
//	actualExtent.height = (std::max)(
//		capabilities.minImageExtent.height,
//		(std::min)(capabilities.maxImageExtent.height, actualExtent.height));
//
//	return actualExtent;
//}
//
//u32 SwapChain::chooseMinImageCount(VkSurfaceCapabilitiesKHR capabilities)
//{
//	u32 imageCount = capabilities.minImageCount + 1;
//	if (capabilities.maxImageCount > 0 &&
//		imageCount > capabilities.maxImageCount)
//	{
//		imageCount = capabilities.maxImageCount;
//	}
//
//	return imageCount;
//}
//
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
//
//void SwapChain::cleanup()
//{
//	for (auto imageView : imageViews)
//	{
//		vkDestroyImageView(mDevice, imageView, nullptr);
//	}
//
//	vkDestroySwapchainKHR(mDevice, mSwapChain, nullptr);
//}
//
