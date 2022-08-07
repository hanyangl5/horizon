#include <runtime/function/rhi/vulkan/VulkanPipeline.h>
#include <runtime/function/rhi/vulkan/VulkanShaderProgram.h>

namespace Horizon::RHI {
VulkanPipeline::VulkanPipeline(const VulkanRendererContext &context,
                               const PipelineCreateInfo &pipeline_create_info,
                               VulkanDescriptorSetManager& descriptor_set_manager) noexcept
    : m_context(context), m_descriptor_set_manager(descriptor_set_manager) {
    m_type = pipeline_create_info.type;
}

VulkanPipeline::~VulkanPipeline() noexcept {
    // TODO: destroy pipeline resources, pipeline layout, pipeline
}

void VulkanPipeline::Create() noexcept {

    // at that time parse shader and generate descriptor layout and pipeline
    // layout.
    CreatePipelineLayout();

    switch (m_type) {
    case PipelineType::COMPUTE:
        CreateComputePipeline();
        break;
    case PipelineType::GRAPHICS:
        CreateGraphicsPipeline();
        break;
    case PipelineType::RAY_TRACING:
        // VkRayTracingPipelineCreateInfoKHR r	t_pipeline_create_info{};
        // vkCreateRayTracingPipelinesKHR();
        break;
    default:
        break;
    }
}

void VulkanPipeline::SetShader(ShaderProgram *shader_moudle) noexcept {
    switch (shader_moudle->GetType()) {
    case ShaderType::VERTEX_SHADER:
    // case ShaderType::GEOMETRY_SHADER:
    case ShaderType::PIXEL_SHADER:
        assert(m_type == PipelineType::GRAPHICS);
        // return;
    case ShaderType::COMPUTE_SHADER:
        assert(m_type == PipelineType::COMPUTE);
        // return;
    }

    shader_map[shader_moudle->GetType()] = shader_moudle;
}

void VulkanPipeline::CreateGraphicsPipeline() noexcept {
    VkGraphicsPipelineCreateInfo graphics_pipeline_create_info{};

    CHECK_VK_RESULT(vkCreateGraphicsPipelines(m_context.device, nullptr, 1,
                                              &graphics_pipeline_create_info,
                                              nullptr, &m_pipeline));
}
void VulkanPipeline::CreateComputePipeline() noexcept {
    if (!shader_map[ShaderType::COMPUTE_SHADER]) {
        LOG_ERROR("missing shader: compute shader");
        return;
    }
    auto cs = static_cast<VulkanShaderProgram *>(
        shader_map[ShaderType::COMPUTE_SHADER]);
    VkPipelineShaderStageCreateInfo shader_stage_create_info{};
    shader_stage_create_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stage_create_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    shader_stage_create_info.module = cs->m_shader_module;
    shader_stage_create_info.pName = cs->GetEntryPoint().c_str();

    VkComputePipelineCreateInfo compute_pipeline_create_info{};

    compute_pipeline_create_info.sType =
        VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    compute_pipeline_create_info.flags = 0;
    compute_pipeline_create_info.layout = m_pipeline_layout;
    compute_pipeline_create_info.stage = shader_stage_create_info;
    compute_pipeline_create_info.basePipelineHandle = nullptr;
    compute_pipeline_create_info.basePipelineIndex = 0;
    CHECK_VK_RESULT(vkCreateComputePipelines(m_context.device, nullptr, 1,
                                             &compute_pipeline_create_info,
                                             nullptr, &m_pipeline));
}
void VulkanPipeline::CreatePipelineLayout() noexcept {
    

    const auto &pl = m_descriptor_set_manager.CreateLayouts(shader_map, m_type);


    //VkPipelineLayoutCreateInfo pipeline_layout_create_info{};

    //pipeline_layout_create_info.sType =
    //    VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    //pipeline_layout_create_info.setLayoutCount =
    //    static_cast<u32>(set_layouts.size());
    //pipeline_layout_create_info.pSetLayouts = set_layouts.data();
    //CHECK_VK_RESULT(vkCreatePipelineLayout(m_context.device,
    //                                       &pipeline_layout_create_info,
    //                                       nullptr, &m_pipeline_layout));
}
} // namespace Horizon::RHI