#include <runtime/function/rhi/vulkan/VulkanPipeline.h>
#include <runtime/function/rhi/vulkan/VulkanShaderProgram.h>

namespace Horizon::RHI {
VulkanPipeline::VulkanPipeline(
    const VulkanRendererContext &context,
    const GraphicsPipelineCreateInfo &create_info,
    VulkanDescriptorSetManager &descriptor_set_manager) noexcept
    : m_context(context), m_descriptor_set_manager(descriptor_set_manager) {
    m_create_info.type = PipelineType::GRAPHICS;
    m_create_info.gpci =
        const_cast<GraphicsPipelineCreateInfo *>(std::move(&create_info));
}

VulkanPipeline::VulkanPipeline(
    const VulkanRendererContext &context,
    const ComputePipelineCreateInfo &create_info,
    VulkanDescriptorSetManager &descriptor_set_manager) noexcept
    : m_context(context), m_descriptor_set_manager(descriptor_set_manager) {
    m_create_info.type = PipelineType::COMPUTE;
    m_create_info.cpci =
        const_cast<ComputePipelineCreateInfo *>(std::move(&create_info));
}

VulkanPipeline::~VulkanPipeline() noexcept {
    // TODO: destroy pipeline resources, pipeline layout, pipeline
}

const std::vector<VkDescriptorSet> &
VulkanPipeline::CreatePipelineResources() noexcept {

    // at that time parse shader and generate descriptor layout and pipeline
    // layout.
    CreatePipelineLayout();

    switch (m_create_info.type) {
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
    m_pipeline_layout_desc.sets =
        m_descriptor_set_manager.AllocateDescriptorSets(m_pipeline_layout_desc);
    return m_pipeline_layout_desc.sets;
}

void VulkanPipeline::SetComputeShader(ShaderProgram *cs) noexcept {
    assert(cs->GetType() == ShaderType::COMPUTE_SHADER);
    assert(m_create_info.type == PipelineType::COMPUTE);
    shader_map[ShaderType::COMPUTE_SHADER] = cs;
    CreatePipelineResources();
}

void VulkanPipeline::SetGraphicsShader(ShaderProgram *vs,
                                       ShaderProgram *ps) noexcept {
    assert(vs->GetType() == ShaderType::VERTEX_SHADER);
    assert(ps->GetType() == ShaderType::PIXEL_SHADER);
    assert(m_create_info.type == PipelineType::GRAPHICS);
    shader_map[ShaderType::VERTEX_SHADER] = vs;
    shader_map[ShaderType::VERTEX_SHADER] = ps;
    CreatePipelineResources();
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

    // cache

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

    m_pipeline_layout_desc =
        m_descriptor_set_manager.CreateLayouts(shader_map, m_create_info.type);
    std::vector<VkDescriptorSetLayout> layouts;
    layouts.reserve(m_pipeline_layout_desc.descriptor_set_hash_key.size());
    for (u32 i = 0; i < m_pipeline_layout_desc.descriptor_set_hash_key.size();
         i++) {
        auto key = m_pipeline_layout_desc.descriptor_set_hash_key[i];
        layouts.emplace_back(m_descriptor_set_manager.FindLayout(key));
    }

    VkPipelineLayoutCreateInfo pipeline_layout_create_info{};

    pipeline_layout_create_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_create_info.setLayoutCount =
        static_cast<u32>(layouts.size());
    pipeline_layout_create_info.pSetLayouts = layouts.data();
    CHECK_VK_RESULT(vkCreatePipelineLayout(m_context.device,
                                           &pipeline_layout_create_info,
                                           nullptr, &m_pipeline_layout));
}
} // namespace Horizon::RHI