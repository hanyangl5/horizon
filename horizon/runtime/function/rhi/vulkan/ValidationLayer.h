#pragma once

#include <vector>
#include <vulkan/vulkan.h>

#include <runtime/function/rhi/RenderContext.h>
#include <runtime/core/log/Log.h>

namespace Horizon {


#ifndef NDEBUG
	static const bool enableValidationLayers = true;
#else
	static const bool enableValidationLayers = false; // disable validation layer in release mode
#endif

	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
		LOG_ERROR("validation layer error: {}", pCallbackData->pMessage);
		return VK_FALSE;
	};

	class ValidationLayer {
	public:
		ValidationLayer() = default;
		~ValidationLayer() = default;
		VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);

		void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);

		bool checkValidationLayerSupport();

		void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& debugUtilsMessengerCreateInfo);

		void setupDebugMessenger(VkInstance instance);

		std::vector<const char*> getRequiredExtensions();
		//private:

		VkDebugUtilsMessengerEXT debugMessenger{};
		const std::vector<const char*> validation_layers{ "VK_LAYER_KHRONOS_validation" };
	};

}