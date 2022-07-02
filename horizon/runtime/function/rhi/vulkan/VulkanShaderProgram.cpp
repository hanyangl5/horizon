#include <runtime/function/rhi/vulkan/VulkanShaderProgram.h>

namespace Horizon::RHI {

	VulkanShaderProgram::VulkanShaderProgram(VkDevice device, ShaderType type,
		const std::string& entry_point, IDxcBlob* shader_byte_code) noexcept :
		m_device(device), ShaderProgram(type, entry_point)
	{
		VkShaderModuleCreateInfo shader_module_create_info{};
		shader_module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		shader_module_create_info.codeSize = shader_byte_code->GetBufferSize();
		shader_module_create_info.pCode = (uint32_t*)shader_byte_code->GetBufferPointer();
		CHECK_VK_RESULT(vkCreateShaderModule(device, &shader_module_create_info, nullptr, &m_shader_module));
		shader_byte_code->Release();
	}

	VulkanShaderProgram::~VulkanShaderProgram() noexcept
	{
		vkDestroyShaderModule(m_device, m_shader_module, nullptr);
	}

}

