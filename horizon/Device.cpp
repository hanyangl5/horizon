#include "Device.h"
#include <vector>
#include <set>
Device::Device(std::shared_ptr<Instance> instance, std::shared_ptr<Surface> surface):mInstance(instance),mSurface(surface)
{
	// enumerate vk devices
	vkEnumeratePhysicalDevices(mInstance->get(), &deviceCount, nullptr);
	if (deviceCount == 0) { spdlog::error("no available device"); }
	vkEnumeratePhysicalDevices(mInstance->get(), &deviceCount, mPhysicalDevices.data());
	pickPhysicalDevice(mInstance->get());
	create(mInstance->getValidationLayer());
}

Device::~Device()
{
	vkDestroyDevice(mDevice, nullptr);
}

VkPhysicalDevice Device::get() const
{
	return mPhysicalDevices[mPhysicalDeviceIndex];
}

bool Device::isDeviceSuitable(VkPhysicalDevice device)
{
	QueueFamilyIndices indices(device,mSurface->get());
	if (indices.completed()) {
		VkPhysicalDeviceProperties deviceProperties;
		vkGetPhysicalDeviceProperties(device, &deviceProperties);
		spdlog::info("using device:{}", deviceProperties.deviceName);
		return true;
	}
	return false;
}

void Device::pickPhysicalDevice(VkInstance instance)
{
	uint32_t deviceCount = 0;
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

void Device::create(const ValidationLayer& validationLayers)
{
	QueueFamilyIndices indices(mPhysicalDevices[mPhysicalDeviceIndex],mSurface->get());

	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

	// The queueFamilyIndex member of each element of pQueueCreateInfos must be unique within pQueueCreateInfos
	// except that two members can share the same queueFamilyIndex if one is a protected-capable queue and one is not a protected-capable queue
	std::set<u32> uniqueQueueFamilies  { indices.getGraphics(), indices.getPresent() };

	float queuePriority = 1.0f;
	for (u32 queueFamily : uniqueQueueFamilies) {
		spdlog::info(queueFamily);
		queueCreateInfos.emplace_back(VkDeviceQueueCreateInfo{ VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO ,nullptr,0,queueFamily,1,&queuePriority});
	}

	VkPhysicalDeviceFeatures deviceFeatures{};

	VkDeviceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.pQueueCreateInfos = queueCreateInfos.data();
	createInfo.queueCreateInfoCount = static_cast<u32>(queueCreateInfos.size());
	createInfo.pEnabledFeatures = &deviceFeatures;
	createInfo.enabledExtensionCount = 0;

	printVkError(vkCreateDevice(mPhysicalDevices[mPhysicalDeviceIndex], &createInfo, nullptr, &mDevice), "create logical device");

	vkGetDeviceQueue(mDevice, indices.getGraphics(), 0, &presentQueue);
	vkGetDeviceQueue(mDevice, indices.getPresent(), 0, &presentQueue);

}

