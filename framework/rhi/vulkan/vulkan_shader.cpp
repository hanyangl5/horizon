#include "vulkan_shader.h"
#include "vulkan_spirv_reflect.h"

namespace Horizon::Backend
{

VulkanShader::VulkanShader(const VulkanRendererContext &context, ShaderType type,
                           std::vector<char> &spirv_code, const char* entry_point) noexcept
    : Shader(type, entry_point), m_context(context)
{
    VkShaderModuleCreateInfo shader_module_create_info{};
    shader_module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader_module_create_info.codeSize = spirv_code.size();
    shader_module_create_info.pCode = reinterpret_cast<const uint32_t *>(spirv_code.data());
    CHECK_VK_RESULT(vkCreateShaderModule(m_context.device, &shader_module_create_info, nullptr, &m_shader_module));
    ReflectSpirvToRootSignature(spirv_code.data(), spirv_code.size(), type, m_reflection);
}

VulkanShader::~VulkanShader() noexcept
{
    vkDestroyShaderModule(m_context.device, m_shader_module, nullptr);
}

} // namespace Horizon::Backend
