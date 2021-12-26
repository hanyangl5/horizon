#include <vulkan/vulkan.hpp>
#include "utils.h"
#include "Instance.h"
class Device
{
public:
    Device(const Instance& instance);
    ~Device();
    VkPhysicalDevice get()const;
private:
    bool isDeviceSuitable(VkPhysicalDevice device);
    // pick the best gpu
    void pickPhysicalDevice(VkInstance instance);
    // use specified gpu
    void setPhysicalDevice(u32 deviceIndex);
private:
    u32 deviceCount;
    u32 mPhysicalDeviceIndex;
    std::vector<VkPhysicalDevice> mPhysicalDevices;
};
