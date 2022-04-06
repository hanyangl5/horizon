#include "ShaderModule.h"

namespace Horizon {

	Shader::Shader(VkDevice device, const std::string& path) :mDevice(device)
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

	std::vector<char> Shader::readFile(const std::string& path)
	{

		std::ifstream file(path, std::ios::ate | std::ios::binary);
		if (!file.is_open()) {
			spdlog::error("failed to open shader file: {}", path);
		}
		size_t fileSize = (size_t)file.tellg();
		std::vector<char> buffer(fileSize);
		file.seekg(0);
		file.read(buffer.data(), fileSize);
		file.close();

		return buffer;

	}

}