#include "Device.h"


Device::Device(std::shared_ptr<Instance> instance)
{
	// enumerate vk devices
	vkEnumeratePhysicalDevices(instance->get(), &deviceCount, nullptr);
	if (deviceCount == 0) { spdlog::error("no available device"); }
	vkEnumeratePhysicalDevices(instance->get(), &deviceCount, mPhysicalDevices.data());
	pickPhysicalDevice(instance->get());
}

Device::~Device()
{
}

VkPhysicalDevice Device::get() const
{
	return mPhysicalDevices[mPhysicalDeviceIndex];
}

bool Device::isDeviceSuitable(VkPhysicalDevice device)
{
	QueueFamilyIndices indices(device);
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

