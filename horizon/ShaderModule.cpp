#include "ShaderModule.h"

Shader::Shader(VkDevice device, const char* path):mDevice(device)
{
	std::vector<char> code = readFile(path);
	if (code.empty()) {
		spdlog::error("open file faild");
	}
	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = code.size();
	createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
	printVkError(vkCreateShaderModule(mDevice, &createInfo, nullptr, &mShaderModule),"create shader module",logLevel::debug);

}

Shader::~Shader()
{
	vkDestroyShaderModule(mDevice,mShaderModule,nullptr);
}

VkShaderModule Shader::get() const
{
	return mShaderModule;
}
