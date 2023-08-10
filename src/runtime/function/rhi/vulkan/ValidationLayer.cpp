#include "ValidationLayer.h"

#ifndef GLFW_INCLUDE_VULKAN
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#endif

namespace Horizon {

VkResult ValidationLayer::CreateDebugUtilsMessengerEXT(VkInstance instance,
                                                       const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
                                                       const VkAllocationCallbacks *pAllocator,
                                                       VkDebugUtilsMessengerEXT *pDebugMessenger) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void ValidationLayer::DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger,
                                                    const VkAllocationCallbacks *pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

bool ValidationLayer::checkValidationLayerSupport() {
    u32 layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    // loop all reqested layers
    for (const auto &layerName : validation_layers) {
        bool layerFound = false;
        // loop all available layers
        for (const auto &layerProperties : availableLayers) {
            if (strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }

        if (!layerFound) {
            return false;
        }
    }

    return true;
}
void ValidationLayer::populateDebugMessengerCreateInfo(
    VkDebugUtilsMessengerCreateInfoEXT &debugUtilsMessengerCreateInfo) {
    debugUtilsMessengerCreateInfo = {};
    debugUtilsMessengerCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    //debugUtilsMessengerCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debugUtilsMessengerCreateInfo.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    //debugUtilsMessengerCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debugUtilsMessengerCreateInfo.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debugUtilsMessengerCreateInfo.pfnUserCallback = debugCallback;
}
void ValidationLayer::setupDebugMessenger(VkInstance instance) {
    if (!enableValidationLayers)
        return;

    VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfo;
    populateDebugMessengerCreateInfo(debugUtilsMessengerCreateInfo);

    CHECK_VK_RESULT(CreateDebugUtilsMessengerEXT(instance, &debugUtilsMessengerCreateInfo, nullptr, &debugMessenger));
}
std::vector<const char *> ValidationLayer::getRequiredExtensions() {
    u32 glfwExtensionCount{0};
    const char **glfwExtensions{};
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector<const char *> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    if (enableValidationLayers) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return extensions;
}
} // namespace Horizon
