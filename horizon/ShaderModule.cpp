#include "ShaderModule.h"

namespace Horizon {

	Shader::Shader(VkDevice device, const char* path) :mDevice(device)
	{
		std::vector<char> code = readFile(path);
		if (code.empty()) {
			spdlog::error("shader code in {} is empty", path);
		}
		VkShaderModuleCreateInfo shaderModuleCreateInfo{};
		shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		shaderModuleCreateInfo.codeSize = code.size();
		shaderModuleCreateInfo.pCode = reinterpret_cast<const u32*>(code.data());
		printVkError(vkCreateShaderModule(mDevice, &shaderModuleCreateInfo, nullptr, &mShaderModule), "create shader module", logLevel::debug);

	}

	Shader::~Shader()
	{
		vkDestroyShaderModule(mDevice, mShaderModule, nullptr);
	}

	VkShaderModule Shader::get() const
	{
		return mShaderModule;
	}

}