
#include "Instance.h"
#include "utils.h"
Instance::Instance()
{
	createInstance();
}

Instance::~Instance()
{
	if (enableValidationLayers) {
		mValidationLayer.DestroyDebugUtilsMessengerEXT(mInstance, mValidationLayer.debugMessenger, nullptr);
	}
	vkDestroyInstance(mInstance, nullptr);
}

VkInstance Instance::get()const
{
	return mInstance;
}

const ValidationLayer& Instance::getValidationLayer() const
{
	return mValidationLayer;
}

void Instance::createInstance()
{

	if (enableValidationLayers && !mValidationLayer.checkValidationLayerSupport()) {
		spdlog::error("validation layers requested, but not available!");
	}

	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Dredgen Engine";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "Dredgen Engine";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_0;

	VkInstanceCreateInfo instanceCreateInfo{};
	instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceCreateInfo.pApplicationInfo = &appInfo;
	instanceCreateInfo.flags = 0;
	auto extensions = mValidationLayer.getRequiredExtensions();
	instanceCreateInfo.enabledExtensionCount = static_cast<u32>(extensions.size());
	instanceCreateInfo.ppEnabledExtensionNames = extensions.data();

	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
	if (enableValidationLayers) {
		instanceCreateInfo.enabledLayerCount = static_cast<u32>(mValidationLayer.validationLayers.size());
		instanceCreateInfo.ppEnabledLayerNames = mValidationLayer.validationLayers.data();
		mValidationLayer.populateDebugMessengerCreateInfo(debugCreateInfo);
		instanceCreateInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
	}
	else {
		instanceCreateInfo.enabledLayerCount = 0;
		instanceCreateInfo.pNext = nullptr;
	}
	VkResult result = vkCreateInstance(&instanceCreateInfo, nullptr, &mInstance);
	printVkError(result, "create vulkan instance");
	if (enableValidationLayers) { mValidationLayer.setupDebugMessenger(mInstance); }
}
