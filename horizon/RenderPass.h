#include <vulkan/vulkan.hpp>
#include "utils.h"
#include "Instance.h"
#include "Device.h"
#include "SwapChain.h"
#include "Surface.h"

// A render pass object represents a collection of attachments, subpasses, and dependencies between the subpasses, and describes how the attachments are used over the course of the subpasses.
class RenderPass
{
public:
	RenderPass(std::shared_ptr<Instance> instance,
		std::shared_ptr<Device> device,
		std::shared_ptr<Surface> surface,
		std::shared_ptr<SwapChain> swapchain);
	~RenderPass();

private:


private:
	std::shared_ptr<Instance> mInstance;
	std::shared_ptr<Device> mDevice;
	std::shared_ptr<Surface> mSurface;
	std::shared_ptr<SwapChain> mSwapChain;
	VkRenderPass mRenderpass;
};
