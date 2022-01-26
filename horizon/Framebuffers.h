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
    Framebuffers(Device* device,SwapChain* swapchain,Pipeline* pipeline);
    ~Framebuffers();
    VkFramebuffer get(u32 i)const;
private:
    void createFrameBuffers();
private:

    SwapChain* mSwapChain;
    Device* mDevice = nullptr;
    Pipeline* mPipeline = nullptr;
    std::vector<VkFramebuffer> mSwapChainFrameBuffers;
};
