#pragma once
#include <vector>
#include <memory>
#include <vulkan/vulkan.hpp>	
#include "Device.h"
#include "Instance.h"
#include "utils.h"
class Window;
class SwapChain {

public:

	SwapChain(std::shared_ptr<Device> device, std::shared_ptr<Surface> surface, std::shared_ptr<Window> window);

	~SwapChain();

	VkSwapchainKHR get() const;

	std::vector<VkImageView> getImageViews() const;

	VkImageView getImageView(u32 i)const;

	VkExtent2D getExtent() const;

	u32 getImageCount() const;

	VkFormat getImageFormat() const;

	void recreate(VkExtent2D newExtent);

private:
	void create();

	VkSurfaceFormatKHR chooseSurfaceFormat(std::vector<VkSurfaceFormatKHR> availableFormats) const;

	VkPresentModeKHR choosePresentMode(std::vector<VkPresentModeKHR> availablePresentModes) const;

	VkExtent2D chooseExtent(VkSurfaceCapabilitiesKHR capabilities);

	static u32 chooseMinImageCount(VkSurfaceCapabilitiesKHR capabilities);

	void saveImages(u32 imageCount);

	//void createImageViews();

	void cleanup();

private:

	const VkSurfaceFormatKHR  PREFERRED_PRESENT_FORMAT = { VK_FORMAT_B8G8R8A8_UNORM , VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
	const VkPresentModeKHR PREFERRED_PRESENT_MODE = VK_PRESENT_MODE_MAILBOX_KHR;
	std::shared_ptr<Device> mDevice;
	std::shared_ptr<Surface> mSurface;
	std::shared_ptr<Window> mWindow;
	VkSwapchainKHR mSwapChain;
	// swap extent is the resolution of swap chain images
	VkExtent2D mExtent;
	VkFormat mImageFormat;
	std::vector<VkImage> images; // handle of swapchain images
	std::vector<VkImageView> imageViews;



};

