#include "VulkanShader.h"

#include <filesystem>

namespace Horizon::Backend {

VulkanShader::VulkanShader(const VulkanRendererContext &context, ShaderType type, Container::Array<char> &spirv_code,
                           const std::filesystem::path &rsd_path) noexcept
    : m_context(context), Shader(type, rsd_path) {
    VkShaderModuleCreateInfo shader_module_create_info{};
    shader_module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader_module_create_info.codeSize = spirv_code.size();
    shader_module_create_info.pCode = (uint32_t *)spirv_code.data();
    CHECK_VK_RESULT(vkCreateShaderModule(m_context.device, &shader_module_create_info, nullptr, &m_shader_module));

}

VulkanShader::~VulkanShader() noexcept {
    vkDestroyShaderModule(m_context.device, m_shader_module, nullptr);
}

} // namespace Horizon::Backend
