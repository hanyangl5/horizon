#include <runtime/function/rhi/vulkan/VulkanShaderProgram.h>

namespace Horizon::RHI {

VulkanShaderProgram::VulkanShaderProgram(const VulkanRendererContext &context, ShaderType type,
                                         const std::string &entry_point, IDxcBlob *shader_byte_code) noexcept
    : m_context(context), ShaderProgram(type, entry_point) {
    VkShaderModuleCreateInfo shader_module_create_info{};
    shader_module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader_module_create_info.codeSize = shader_byte_code->GetBufferSize();
    shader_module_create_info.pCode = (uint32_t *)shader_byte_code->GetBufferPointer();
    CHECK_VK_RESULT(vkCreateShaderModule(m_context.device, &shader_module_create_info, nullptr, &m_shader_module));
    shader_byte_code->AddRef();
    m_shader_byte_code = shader_byte_code;

    // shader_byte_code->Release();
}

VulkanShaderProgram::~VulkanShaderProgram() noexcept {
    vkDestroyShaderModule(m_context.device, m_shader_module, nullptr);
}

} // namespace Horizon::RHI
