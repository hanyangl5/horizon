#include "Instance.h"

#include <runtime/core/log/Log.h>

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

	VkInstance Instance::Get()const
	{
		return m_instance;
	}

	const ValidationLayer& Instance::getValidationLayer() const
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
}