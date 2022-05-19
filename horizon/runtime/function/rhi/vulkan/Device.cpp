#include "Device.h"

#include <vector>
#include <set>

#include <runtime/core/log/Log.h>

#include "QueueFamilyIndices.h"
#include "SurfaceSupportDetails.h"

namespace Horizon {
	Instance::Instance()
	{
		createInstance();
	}

	Instance::~Instance()
	{
		if (enableValidationLayers) {
			m_validation_layer.DestroyDebugUtilsMessengerEXT(m_instance, m_validation_layer.debugMessenger, nullptr);
		}
		vkDestroyInstance(m_instance, nullptr);
	}

	VkInstance Instance::Get()const noexcept 
	{
		return m_instance;
	}

	const ValidationLayer& Instance::getValidationLayer() const noexcept 
	{
		return m_validation_layer;
	}

	void Instance::createInstance()
	{

		if (enableValidationLayers && !m_validation_layer.checkValidationLayerSupport()) {
			LOG_ERROR("validation layers requested, but not available!");
		}

		VkApplicationInfo appInfo{};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "Horizon Engine";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "Horizon Engine";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_1;

		VkInstanceCreateInfo instance_create_info{};
		instance_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		instance_create_info.pApplicationInfo = &appInfo;
		instance_create_info.flags = 0;
		auto& extensions = m_validation_layer.getRequiredExtensions();
		instance_create_info.enabledExtensionCount = static_cast<u32>(extensions.size());
		instance_create_info.ppEnabledExtensionNames = extensions.data();

		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
		if (enableValidationLayers) {
			instance_create_info.enabledLayerCount = static_cast<u32>(m_validation_layer.validation_layers.size());
			instance_create_info.ppEnabledLayerNames = m_validation_layer.validation_layers.data();
			m_validation_layer.populateDebugMessengerCreateInfo(debugCreateInfo);
			instance_create_info.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
		}
		else {
			instance_create_info.enabledLayerCount = 0;
			instance_create_info.pNext = nullptr;
		}
		VkResult result = vkCreateInstance(&instance_create_info, nullptr, &m_instance);
		CHECK_VK_RESULT(result);
		if (enableValidationLayers) { m_validation_layer.setupDebugMessenger(m_instance); }
	}

	Surface::Surface(std::shared_ptr<Instance> instance, std::shared_ptr<Window> window) :m_instance(instance), m_window(window)
	{
		createSurface();
	}

	Surface::~Surface()
	{
		vkDestroySurfaceKHR(m_instance->Get(), m_surface, nullptr);
	}

	VkSurfaceKHR Surface::Get() const noexcept 
	{
		return m_surface;
	}

	void Surface::createSurface()
	{
		CHECK_VK_RESULT(glfwCreateWindowSurface(m_instance->Get(), m_window->getWindow(), nullptr, &m_surface));
	}
	
	Device::Device(std::shared_ptr<Window> window)noexcept
	{

		m_instance = std::make_shared<Instance>();
		m_surface = std::make_shared<Surface>(m_instance, window);

		// enumerate vk devices
		vkEnumeratePhysicalDevices(m_instance->Get(), &device_count, nullptr);
		if (device_count == 0) { LOG_ERROR("no available device"); }
		vkEnumeratePhysicalDevices(m_instance->Get(), &device_count, m_physical_devices.data());
		pickPhysicalDevice(m_instance->Get());
		createDevice(m_instance->getValidationLayer());
	}

	Device::~Device()noexcept
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

	std::shared_ptr<Surface> Device::GetSurface() const noexcept
	{
		return m_surface;
	}

}