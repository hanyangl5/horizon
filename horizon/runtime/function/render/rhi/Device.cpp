#include "Device.h"

#include <vector>
#include <set>

#include <runtime/core/log/Log.h>

#include "QueueFamilyIndices.h"
#include "SurfaceSupportDetails.h"

namespace Horizon {
	Device::Device(std::shared_ptr<Instance> instance, std::shared_ptr<Surface> surface) :m_instance(instance), m_surface(surface)
	{
		// enumerate vk devices
		vkEnumeratePhysicalDevices(m_instance->Get(), &device_count, nullptr);
		if (device_count == 0) { LOG_ERROR("no available device"); }
		vkEnumeratePhysicalDevices(m_instance->Get(), &device_count, m_physical_devices.data());
		pickPhysicalDevice(m_instance->Get());
		createDevice(m_instance->getValidationLayer());
	}

	Device::~Device()
	{
		vkDestroyDevice(m_device, nullptr);
	}

	VkPhysicalDevice Device::getPhysicalDevice() const noexcept 
	{
		return m_physical_devices[m_physical_device_index];
	}

	VkDevice Device::Get() const noexcept 
	{
		return m_device;
	}

	VkQueue Device::getGraphicQueue() const noexcept 
	{
		return m_graphics_queue;
	}

	VkQueue Device::getPresnetQueue() const noexcept 
	{
		return m_present_queue;
	}

	bool Device::isDeviceSuitable(VkPhysicalDevice device)
	{
		QueueFamilyIndices indices(device, m_surface->Get());
		SurfaceSupportDetails details(device, m_surface->Get());
		if (indices.completed() && details.suitable() && checkDeviceExtensionSupport(device)) {
			VkPhysicalDeviceProperties device_properties;
			vkGetPhysicalDeviceProperties(device, &device_properties);
			LOG_INFO("using device:{}", device_properties.deviceName);
			m_queue_family_indices = indices;
			return true;
		}
		return false;
	}

	void Device::pickPhysicalDevice(VkInstance instance)
	{
		u32 device_count = 0;
		vkEnumeratePhysicalDevices(instance, &device_count, nullptr);

		if (device_count == 0) {
			LOG_ERROR("failed to find GPUs with Vulkan support!");
		}
		m_physical_devices.resize(device_count);
		vkEnumeratePhysicalDevices(instance, &device_count, m_physical_devices.data());


		// pick gpu
		for (u32 device_index = 0; device_index < m_physical_devices.size(); device_index++) {
			if (isDeviceSuitable(m_physical_devices[device_index])) {
				m_physical_device_index = device_index;
				break;
			}
		}

		if (m_physical_devices[m_physical_device_index] == VK_NULL_HANDLE) {
			LOG_ERROR("failed to find a suitable GPU!");
		}
	}

	void Device::setPhysicalDevice(u32 device_index)
	{
		m_physical_device_index = device_index;
	}

	void Device::createDevice(const ValidationLayer& validation_layers)
	{

		std::vector<VkDeviceQueueCreateInfo> device_queue_create_info{};

		// The queueFamilyIndex member of each element of pQueueCreateInfos must be unique within pQueueCreateInfos
		// except that two members can share the same queueFamilyIndex if one is a protected-capable queue and one is not a protected-capable queue
		std::set<u32> unique_queue_families{ m_queue_family_indices.getGraphics(), m_queue_family_indices.getPresent() };

		f32 queue_priority = 1.0f;
		for (u32 queue_family : unique_queue_families) {
			device_queue_create_info.emplace_back(VkDeviceQueueCreateInfo{ VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO ,nullptr,0,queue_family,1,&queue_priority });
		}

		VkPhysicalDeviceFeatures deviceFeatures{};

		VkDeviceCreateInfo device_create_info{};
		device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		device_create_info.pQueueCreateInfos = device_queue_create_info.data();
		device_create_info.queueCreateInfoCount = static_cast<u32>(device_queue_create_info.size());
		device_create_info.pEnabledFeatures = &deviceFeatures;
		device_create_info.enabledExtensionCount = static_cast<u32>(m_device_extensions.size());
		device_create_info.ppEnabledExtensionNames = m_device_extensions.data();

		CHECK_VK_RESULT(vkCreateDevice(m_physical_devices[m_physical_device_index], &device_create_info, nullptr, &m_device));

		vkGetDeviceQueue(m_device, m_queue_family_indices.getGraphics(), 0, &m_graphics_queue);
		vkGetDeviceQueue(m_device, m_queue_family_indices.getPresent(), 0, &m_present_queue);

	}

	bool Device::checkDeviceExtensionSupport(VkPhysicalDevice device) {
		u32 extension_count;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr);

		std::vector<VkExtensionProperties> available_extensions(extension_count);
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, available_extensions.data());

		std::set<std::string> required_extensions(m_device_extensions.begin(), m_device_extensions.end());

		for (const auto& extension : available_extensions) {
			required_extensions.erase(extension.extensionName);
		}

		return required_extensions.empty();
	}

	QueueFamilyIndices Device::getQueueFamilyIndices() const noexcept 
	{
		return m_queue_family_indices;
	}

}