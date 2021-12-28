#pragma once
#include <vulkan/vulkan.hpp>
#include "utils.h"
#include "Instance.h"
#include "QueueFamily.h"
#include "Surface.h"
#include <memory>
class Device
{
public:
    Device(std::shared_ptr<Instance> instance,std::shared_ptr<Surface> surface);
    ~Device();
    VkPhysicalDevice get()const;
private:
    bool isDeviceSuitable(VkPhysicalDevice device);
    // pick the best gpu
    void pickPhysicalDevice(VkInstance instance);
    // use specified gpu
    void setPhysicalDevice(u32 deviceIndex);

    void create(const ValidationLayer& validationLayers);
private:
    u32 deviceCount;
    i32 mPhysicalDeviceIndex = -1;
    std::vector<VkPhysicalDevice> mPhysicalDevices;
    VkDevice mDevice;
    VkQueue graphicsQueue, presentQueue;
    std::shared_ptr<Instance> mInstance;
    std::shared_ptr<Surface> mSurface;
};
