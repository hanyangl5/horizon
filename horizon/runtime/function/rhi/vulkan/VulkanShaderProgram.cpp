#include <runtime/function/rhi/vulkan/VulkanShaderProgram.h>

namespace Horizon::RHI {

VulkanShaderProgram::VulkanShaderProgram(const VulkanRendererContext &context,
                                         ShaderType type,
                                         const std::string &entry_point,
                                         IDxcBlob *shader_byte_code) noexcept
    : m_context(context), ShaderProgram(type, entry_point) {
    VkShaderModuleCreateInfo shader_module_create_info{};
    shader_module_create_info.sType =
        VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader_module_create_info.codeSize = shader_byte_code->GetBufferSize();
    shader_module_create_info.pCode =
        (uint32_t *)shader_byte_code->GetBufferPointer();
    CHECK_VK_RESULT(vkCreateShaderModule(m_context.device,
                                         &shader_module_create_info, nullptr,
                                         &m_shader_module));
    ReflectDescriptorSetLayout(shader_byte_code->GetBufferPointer(),
                               shader_byte_code->GetBufferSize());

    shader_byte_code->Release();
}

VulkanShaderProgram::~VulkanShaderProgram() noexcept {
    vkDestroyShaderModule(m_context.device, m_shader_module, nullptr);
}

void VulkanShaderProgram::ReflectDescriptorSetLayout(void *spirv, u32 size) {
    // reflection data
    // Generate reflection data for a shader
    SpvReflectShaderModule module;
    SpvReflectResult result =
        spvReflectCreateShaderModule(size, spirv, &module);
    assert(result == SPV_REFLECT_RESULT_SUCCESS);

    // Enumerate and extract shader's input variables
    uint32_t var_count = 0;
    result = spvReflectEnumerateInputVariables(&module, &var_count, NULL);
    assert(result == SPV_REFLECT_RESULT_SUCCESS);

    uint32_t count = 0;
    result = spvReflectEnumerateDescriptorSets(&module, &count, NULL);
    assert(result == SPV_REFLECT_RESULT_SUCCESS);

    std::vector<SpvReflectDescriptorSet *> sets(count);
    result = spvReflectEnumerateDescriptorSets(&module, &count, sets.data());
    assert(result == SPV_REFLECT_RESULT_SUCCESS);
}

} // namespace Horizon::RHI
