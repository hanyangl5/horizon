#pragma once

#include <vulkan/vulkan.hpp>
#include "utils.h"
#include "ValidationLayer.h"
class Instance
{
public:
	Instance();
	~Instance();
	VkInstance get()const;
	const ValidationLayer& getValidationLayer()const;
private:
	void createInstance();
private:
	VkInstance mInstance;
	u32 mExtensionCount = 0;
	std::vector<VkExtensionProperties> mExtensions;
	ValidationLayer mValidationLayer;
};
