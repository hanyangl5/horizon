
#include "Instance.h"
#include "utils.h"
Instance::Instance()
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

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.flags = 0;
    auto extensions = mValidationLayer.getRequiredExtensions();
    createInfo.enabledExtensionCount = static_cast<u32>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if (enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<u32>(mValidationLayer.validationLayers.size());
        createInfo.ppEnabledLayerNames = mValidationLayer.validationLayers.data();
        mValidationLayer.populateDebugMessengerCreateInfo(debugCreateInfo);
        createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
    }
    else {
        createInfo.enabledLayerCount = 0;
        createInfo.pNext = nullptr;
    }
    VkResult result = vkCreateInstance(&createInfo, nullptr, &mInstance);

    printVkError(result, "create vulkan instance");

    if(enableValidationLayers) {mValidationLayer.setupDebugMessenger(mInstance);}
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
