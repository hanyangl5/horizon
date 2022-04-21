#include "ShaderModule.h"

#include <fstream>
#include <vector>

#include <runtime/core/log/Log.h>

namespace Horizon {

	Shader::Shader(VkDevice device, const std::string& path) :m_device(device)
	{
		std::vector<char> code = readFile(path);
		if (code.empty()) {
			LOG_ERROR("shader code in {} is empty", path);
		}
		VkShaderModuleCreateInfo shaderModuleCreateInfo{};
		shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		shaderModuleCreateInfo.codeSize = code.size();
		shaderModuleCreateInfo.pCode = reinterpret_cast<const u32*>(code.data());
		CHECK_VK_RESULT(vkCreateShaderModule(m_device, &shaderModuleCreateInfo, nullptr, &m_shader_module));

	}

	Shader::~Shader()
	{
		vkDestroyShaderModule(m_device, m_shader_module, nullptr);
	}

	VkShaderModule Shader::Get() const noexcept 
	{
		return m_shader_module;
	}

	std::vector<char> Shader::readFile(const std::string& path)
	{

		std::ifstream file(path, std::ios::ate | std::ios::binary);
		if (!file.is_open()) {
			LOG_ERROR("failed to open shader file: {}", path);
		}
		size_t fileSize = (size_t)file.tellg();
		std::vector<char> buffer(fileSize);
		file.seekg(0);
		file.read(buffer.data(), fileSize);
		file.close();

		return buffer;

	}

}