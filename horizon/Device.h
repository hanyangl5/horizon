#pragma once
#include <vulkan/vulkan.hpp>
#include "utils.h"
#include "Instance.h"
#include "QueueFamily.h"
#include <memory>
class Device
{
public:
    Device(std::shared_ptr<Instance> instance);
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
};
