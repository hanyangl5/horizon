#include <GLFW/glfw3.h>
#include "Instance.h"
#include "utils.h"
Instance::Instance(const uint32_t enabledExtensionCount,const std::vector<std::string> & enabledExtension)
{
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Dredgen Engine";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "Dredgen Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = enabledExtensionCount;

    char** cstrings = new char* [enabledExtension.size()];
    for (int index = 0; index < enabledExtension.size(); index++) {
        cstrings[index] = const_cast<char*>(enabledExtension[index].c_str());
    }

    createInfo.ppEnabledExtensionNames = cstrings;

    VkResult result = vkCreateInstance(&createInfo, nullptr, &mInstance);
    PrintVkError(result, "Create VkInstance");

}

Instance::~Instance()
{
    
    vkDestroyInstance(mInstance, nullptr);
}

VkInstance Instance::get()const
{
    return mInstance;
}
