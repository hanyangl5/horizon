#include "Device.h"
#include <vector>
#include <set>
#include "QueueFamilyIndices.h"
#include "SurfaceSupportDetails.h"
Device::Device(std::shared_ptr<Instance> instance, std::shared_ptr<Surface> surface):mInstance(instance),mSurface(surface)
{
	// enumerate vk devices
	vkEnumeratePhysicalDevices(mInstance->get(), &deviceCount, nullptr);
	if (deviceCount == 0) { spdlog::error("no available device"); }
	vkEnumeratePhysicalDevices(mInstance->get(), &deviceCount, mPhysicalDevices.data());
	pickPhysicalDevice(mInstance->get());
	createDevice(mInstance->getValidationLayer());
}

Device::~Device()
{
	vkDestroyDevice(mDevice, nullptr);
}

VkPhysicalDevice Device::getPhysicalDevice() const
{
	return mPhysicalDevices[mPhysicalDeviceIndex];
}

VkDevice Device::get() const
{
	return mDevice;
}

bool Device::isDeviceSuitable(VkPhysicalDevice device)
{
	QueueFamilyIndices indices(device,mSurface->get());
	SurfaceSupportDetails details(device, mSurface->get());
	if (indices.completed() && details.suitable()&& checkDeviceExtensionSupport(device)) {
		VkPhysicalDeviceProperties deviceProperties;
		vkGetPhysicalDeviceProperties(device, &deviceProperties);
		spdlog::info("using device:{}", deviceProperties.deviceName);
		mQueueFamilyIndices = indices;
		return true;
	}
	return false;
}

void Device::pickPhysicalDevice(VkInstance instance)
{
	u32 deviceCount = 0;
	vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

	if (deviceCount == 0) {
		spdlog::error("failed to find GPUs with Vulkan support!");
	}
	mPhysicalDevices.resize(deviceCount);
	vkEnumeratePhysicalDevices(instance, &deviceCount, mPhysicalDevices.data());


	// pick gpu
	for (u32 deviceIndex = 0; deviceIndex < mPhysicalDevices.size(); deviceIndex++) {
		if (isDeviceSuitable(mPhysicalDevices[deviceIndex])) {
			mPhysicalDeviceIndex = deviceIndex;
			break;
		}
	}

	if (mPhysicalDevices[mPhysicalDeviceIndex] == VK_NULL_HANDLE) {
		spdlog::error("failed to find a suitable GPU!");
	}
}

void Device::setPhysicalDevice(u32 deviceIndex)
{
	mPhysicalDeviceIndex = deviceIndex;
}

void Device::createDevice(const ValidationLayer& validationLayers)
{

	std::vector<VkDeviceQueueCreateInfo> deviceQueueCreateInfos{};

	// The queueFamilyIndex member of each element of pQueueCreateInfos must be unique within pQueueCreateInfos
	// except that two members can share the same queueFamilyIndex if one is a protected-capable queue and one is not a protected-capable queue
	std::set<u32> uniqueQueueFamilies { mQueueFamilyIndices.getGraphics(), mQueueFamilyIndices.getPresent() };

	f32 queuePriority = 1.0f;
	for (u32 queueFamily : uniqueQueueFamilies) {
		deviceQueueCreateInfos.emplace_back(VkDeviceQueueCreateInfo{ VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO ,nullptr,0,queueFamily,1,&queuePriority});
	}

	VkPhysicalDeviceFeatures deviceFeatures{};

	VkDeviceCreateInfo deviceCreateInfo{};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.pQueueCreateInfos = deviceQueueCreateInfos.data();
	deviceCreateInfo.queueCreateInfoCount = static_cast<u32>(deviceQueueCreateInfos.size());
	deviceCreateInfo.pEnabledFeatures = &deviceFeatures;
	deviceCreateInfo.enabledExtensionCount = static_cast<u32>(deviceExtensions.size());
	deviceCreateInfo.ppEnabledExtensionNames=deviceExtensions.data();
	
	printVkError(vkCreateDevice(mPhysicalDevices[mPhysicalDeviceIndex], &deviceCreateInfo, nullptr, &mDevice), "create logical device");

	vkGetDeviceQueue(mDevice, mQueueFamilyIndices.getGraphics(), 0, &presentQueue);
	vkGetDeviceQueue(mDevice, mQueueFamilyIndices.getPresent(), 0, &presentQueue);

}


bool Device::checkDeviceExtensionSupport(VkPhysicalDevice device){
    u32 extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

    for (const auto& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

QueueFamilyIndices Device::getQueueFamilyIndices()
{
	return mQueueFamilyIndices;
}
