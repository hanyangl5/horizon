//#pragma once
//#include <vector>
//#include <memory>
//#include <vulkan/vulkan.hpp>	
//#include "Device.h"
//#include "Instance.h"
//#include "utils.h"
//class SwapChain {
//
//public:
//
//	SwapChain(std::shared_ptr<Device> device, VkSurfaceKHR surface, VkExtent2D surfaceExtent);
//
//	~SwapChain();
//
//	VkSwapchainKHR get() const;
//
//	std::vector<VkImageView> getImageViews() const;
//	VkImageView getImageView(u32 i)const;
//	VkExtent2D getExtent() const;
//
//	u32 getImageCount() const;
//
//	VkFormat getImageFormat() const;
//
//	void recreate(VkExtent2D newExtent);
//
//
//private:
//
//	const VkSurfaceFormatKHR PREFERRED_PRESENT_FORMAT = { VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
//
//	const VkPresentModeKHR PREFERRED_PRESENT_MODE = VK_PRESENT_MODE_MAILBOX_KHR;
//	
//	VkDevice mDevice;
//
//	VkSurfaceKHR mSurface;
//
//	VkSwapchainKHR mSwapChain;
//
//	VkExtent2D extent;
//	VkFormat imageFormat;
//
//	std::vector<VkImage> images;
//	std::vector<VkImageView> imageViews;
//
//
//	void create(VkExtent2D surfaceExtent);
//
//	VkSurfaceFormatKHR chooseSurfaceFormat(std::vector<VkSurfaceFormatKHR> availableFormats) const;
//
//	VkPresentModeKHR choosePresentMode(std::vector<VkPresentModeKHR> availablePresentModes) const;
//
//	static VkExtent2D chooseExtent(VkSurfaceCapabilitiesKHR capabilities, VkExtent2D actualExtent);
//
//	static u32 chooseMinImageCount(VkSurfaceCapabilitiesKHR capabilities);
//
//	void saveImages(u32 imageCount);
//
//	void createImageViews();
//
//	void cleanup();
//
//};
//
