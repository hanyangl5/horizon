#pragma once

#include <vector>

#include <vulkan/vulkan.hpp>

#include "utils.h"
#include "ValidationLayer.h"

namespace Horizon {

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
}