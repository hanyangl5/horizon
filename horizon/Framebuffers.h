#pragma once
#include <vector>
#include <memory>
#include "Device.h"
#include "SwapChain.h"
#include "Pipeline.h"
#include <vulkan/vulkan.hpp>

//we have to create a framebuffer for all of the images in the swap chain and use the one that corresponds to the retrieved image at drawing time.
class Framebuffers
{
public:
    Framebuffers(std::shared_ptr<Device> device,std::shared_ptr<SwapChain> swapchain,std::shared_ptr<Pipeline> pipeline);
    ~Framebuffers();
    VkFramebuffer get(u32 i)const;
private:
    void createFrameBuffers();
private:

    std::shared_ptr<SwapChain> mSwapChain;
    std::shared_ptr<Device> mDevice;
    std::shared_ptr<Pipeline> mPipeline;
    std::vector<VkFramebuffer> mSwapChainFrameBuffers;
};
