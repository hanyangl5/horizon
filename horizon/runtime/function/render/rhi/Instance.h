#pragma once

#include <vector>

#include <vulkan/vulkan.hpp>

#include <runtime/core/utils/utils.h>
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
		VkInstance m_instance;
		u32 m_extension_count = 0;
		std::vector<VkExtensionProperties> m_extensions;
		ValidationLayer m_validation_layer;
	};
}