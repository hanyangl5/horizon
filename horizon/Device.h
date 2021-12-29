#pragma once
#include <vulkan/vulkan.hpp>
#include "utils.h"
#include "Instance.h"
#include "Surface.h"
#include <memory>
class Device
{
public:
    Device(std::shared_ptr<Instance> instance, std::shared_ptr<Surface> surface);
    ~Device();
    VkPhysicalDevice getPhysicalDevice() const;
    VkDevice get()const;
private:
    bool isDeviceSuitable(VkPhysicalDevice device);
    // pick the best gpu
    void pickPhysicalDevice(VkInstance instance);
    // use specified gpu
    void setPhysicalDevice(u32 deviceIndex);

    void create(const ValidationLayer &validationLayers);

    bool checkDeviceExtensionSupport(VkPhysicalDevice device);

private:
    u32 deviceCount;
    i32 mPhysicalDeviceIndex = -1;
    std::vector<VkPhysicalDevice> mPhysicalDevices;
    VkDevice mDevice{};
    VkQueue graphicsQueue, presentQueue;
    std::shared_ptr<Instance> mInstance;
    std::shared_ptr<Surface> mSurface;
    const std::vector<const char *> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
};
