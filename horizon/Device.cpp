#include "Device.h"


Device::Device(std::shared_ptr<Instance> instance)
{
	// enumerate vk devices
	vkEnumeratePhysicalDevices(instance->get(), &deviceCount, nullptr);
	if (deviceCount == 0) { spdlog::error("no available device"); }
	vkEnumeratePhysicalDevices(instance->get(), &deviceCount, mPhysicalDevices.data());
	pickPhysicalDevice(instance->get());
	create(instance->getValidationLayer());
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

void Device::create(const ValidationLayer& validationLayers)
{
	QueueFamilyIndices indices(mPhysicalDevices[mPhysicalDeviceIndex]);

	VkDeviceQueueCreateInfo queueCreateInfo{};
	queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueCreateInfo.queueFamilyIndex = indices.getGraphics();
	queueCreateInfo.queueCount = 1;
	float queuePriority = 1.0f;
	queueCreateInfo.pQueuePriorities = &queuePriority;
	VkPhysicalDeviceFeatures deviceFeatures{};
	VkDeviceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.pQueueCreateInfos = &queueCreateInfo;
	createInfo.queueCreateInfoCount = 1;

	createInfo.pEnabledFeatures = &deviceFeatures;
	createInfo.enabledExtensionCount = 0;

	if (enableValidationLayers) {
		createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.validationLayers.size());
		createInfo.ppEnabledLayerNames = validationLayers.validationLayers.data();
	}
	else {
		createInfo.enabledLayerCount = 0;
	}
	VkResult res = vkCreateDevice(mPhysicalDevices[mPhysicalDeviceIndex], &createInfo, nullptr, &mDevice);
	PrintVkError(res, "create logical device");
}

