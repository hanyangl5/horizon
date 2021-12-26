
#include "Instance.h"
#include "utils.h"
#include <spdlog/spdlog.h>
Instance::Instance()
{
    //vkEnumerateInstanceExtensionProperties(nullptr, &mExtensionCount,nullptr);
    //mExtensions.resize(mExtensionCount);
    //vkEnumerateInstanceExtensionProperties(nullptr, &mExtensionCount, mExtensions.data());
    //spdlog::info("available Instance extensions: ");

    //for (const auto& extension : mExtensions) {
    //    spdlog::info(extension.extensionName );
    //}
    //VkApplicationInfo appInfo{};
    //appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    //appInfo.pApplicationName = "Dredgen Engine";
    //appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    //appInfo.pEngineName = "Dredgen Engine";
    //appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    //appInfo.apiVersion = VK_API_VERSION_1_0;

    //VkInstanceCreateInfo createInfo{};
    //createInfo.sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    //createInfo.pApplicationInfo = &appInfo;
    //createInfo.enabledExtensionCount = enabledExtensionCount;
    //std::vector<char const*> pEnabledExtension(enabledExtensions.size());
    //std::transform(std::begin(enabledExtensions), std::end(enabledExtensions), std::begin(pEnabledExtension),
    //    std::mem_fn(&std::string::c_str));
    //createInfo.ppEnabledExtensionNames = pEnabledExtension.data();
    //createInfo.enabledLayerCount=0;

    //VkResult result = vkCreateInstance(&createInfo, nullptr, &mInstance);
    //PrintVkError(result, "Create VkInstance");
    
    //-------------------------
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
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if (enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(mValidationLayer.validationLayers.size());
        createInfo.ppEnabledLayerNames = mValidationLayer.validationLayers.data();
        mValidationLayer.populateDebugMessengerCreateInfo(debugCreateInfo);
        createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
    }
    else {
        createInfo.enabledLayerCount = 0;
        createInfo.pNext = nullptr;
    }
    VkResult result = vkCreateInstance(&createInfo, nullptr, &mInstance);
    PrintVkError(result, "Create VkInstance");

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
