#pragma once

#include <vector>
#include <memory>

#include <vulkan/vulkan.h>

#include "Device.h"

#include <runtime/core/window/Window.h>
#include <runtime/function/rhi/RenderContext.h>


namespace Horizon {

	class SwapChain {

	public:

		SwapChain(RenderContext& render_context, std::shared_ptr<Device> device, std::shared_ptr<Surface> surface);

		~SwapChain();

		VkSwapchainKHR Get() const noexcept;

		std::vector<VkImageView> getImageViews() const noexcept;

		VkImageView getImageView(u32 i) const noexcept;

		VkFormat getImageFormat() const noexcept;


		void recreate(VkExtent2D newExtent);

	private:
		void createSwapChain();

		VkSurfaceFormatKHR chooseSurfaceFormat(std::vector<VkSurfaceFormatKHR> availableFormats) const noexcept;

		VkPresentModeKHR choosePresentMode(std::vector<VkPresentModeKHR> availablePresentModes) const noexcept;

		VkExtent2D chooseExtent(VkSurfaceCapabilitiesKHR capabilities);

		static u32 chooseMinImageCount(VkSurfaceCapabilitiesKHR capabilities);

		void createImageViews();

		void cleanup();


	private:
		RenderContext& m_render_context;		
		const VkSurfaceFormatKHR  PREFERRED_PRESENT_FORMAT = { VK_FORMAT_R16G16B16A16_UNORM , VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
		const VkPresentModeKHR PREFERRED_PRESENT_MODE = VK_PRESENT_MODE_MAILBOX_KHR;
		std::shared_ptr<Device> m_device = nullptr;
		std::shared_ptr<Surface> m_surface = nullptr;
		std::shared_ptr<Window> m_window = nullptr;
		VkSwapchainKHR m_swap_chain;
		//VkExtent2D mExtent;	// swap extent is the resolution of swap chain images
		VkFormat mImageFormat;
		std::vector<VkImage> images; // handle of swapchain images
		std::vector<VkImageView> imageViews; // An image view is quite literally a view into an image. It describes how to access the image and which part of the image to access

		//VkImage depthImage;
		//VkDeviceMemory depthImageMemory;
		//VkImageView depthImageView;
		//VkFormat mDepthFormat;
	};
}

