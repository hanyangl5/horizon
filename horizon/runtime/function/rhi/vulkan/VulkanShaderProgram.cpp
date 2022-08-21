#include <runtime/function/rhi/vulkan/VulkanShaderProgram.h>

namespace Horizon::RHI {

VulkanShaderProgram::VulkanShaderProgram(const VulkanRendererContext &context, ShaderType type, std::vector<char>& spirv_code) noexcept
    : m_context(context), ShaderProgram(type) {
    VkShaderModuleCreateInfo shader_module_create_info{};
    shader_module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader_module_create_info.codeSize = spirv_code.size();
    shader_module_create_info.pCode = (uint32_t *)spirv_code.data();
    CHECK_VK_RESULT(vkCreateShaderModule(m_context.device, &shader_module_create_info, nullptr, &m_shader_module));

    m_spirv_code.swap(spirv_code);
}

VulkanShaderProgram::~VulkanShaderProgram() noexcept {
    vkDestroyShaderModule(m_context.device, m_shader_module, nullptr);
}

} // namespace Horizon::RHI
