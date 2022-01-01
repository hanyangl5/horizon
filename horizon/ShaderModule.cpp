#include "ShaderModule.h"

Shader::Shader(VkDevice device, const char* path):mDevice(device)
{
	std::vector<char> code = readFile(path);
	if (code.empty()) {
		spdlog::error("open file faild");
	}
	VkShaderModuleCreateInfo shaderModuleCreateInfo{};
	shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shaderModuleCreateInfo.codeSize = code.size();
	shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
	printVkError(vkCreateShaderModule(mDevice, &shaderModuleCreateInfo, nullptr, &mShaderModule),"create shader module",logLevel::debug);

}

Shader::~Shader()
{
	vkDestroyShaderModule(mDevice,mShaderModule,nullptr);
}

VkShaderModule Shader::get() const
{
	return mShaderModule;
}
