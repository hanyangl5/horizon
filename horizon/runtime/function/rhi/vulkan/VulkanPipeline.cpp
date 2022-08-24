#include <runtime/function/rhi/vulkan/VulkanPipeline.h>
#include <runtime/function/rhi/vulkan/VulkanShaderProgram.h>

namespace Horizon::RHI {
VulkanPipeline::VulkanPipeline(const VulkanRendererContext &context, const GraphicsPipelineCreateInfo &create_info,
                               VulkanDescriptorSetManager &descriptor_set_manager) noexcept
    : m_context(context), m_descriptor_set_manager(descriptor_set_manager) {
    m_create_info.type = PipelineType::GRAPHICS;
    m_create_info.gpci = const_cast<GraphicsPipelineCreateInfo *>(std::move(&create_info));
}

VulkanPipeline::VulkanPipeline(const VulkanRendererContext &context, const ComputePipelineCreateInfo &create_info,
                               VulkanDescriptorSetManager &descriptor_set_manager) noexcept
    : m_context(context), m_descriptor_set_manager(descriptor_set_manager) {
    m_create_info.type = PipelineType::COMPUTE;
    m_create_info.cpci = const_cast<ComputePipelineCreateInfo *>(std::move(&create_info));
}

VulkanPipeline::~VulkanPipeline() noexcept {
    // TODO: destroy pipeline resources, pipeline layout, pipeline
    vkDestroyPipeline(m_context.device, m_pipeline, nullptr);
    vkDestroyPipelineLayout(m_context.device, m_pipeline_layout, nullptr);
}

const std::vector<VkDescriptorSet> &VulkanPipeline::CreatePipelineResources() {

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
    m_pipeline_layout_desc.sets = m_descriptor_set_manager.AllocateDescriptorSets(m_pipeline_layout_desc);
    return m_pipeline_layout_desc.sets;
}

void VulkanPipeline::SetComputeShader(ShaderProgram *cs) {
    assert(("shader is not compute shader", cs->GetType() == ShaderType::COMPUTE_SHADER));
    assert(("pipeline is not compute shader", m_create_info.type == PipelineType::COMPUTE));
    shader_map[ShaderType::COMPUTE_SHADER] = cs;
    CreatePipelineResources();
}

void VulkanPipeline::SetGraphicsShader(ShaderProgram *vs, ShaderProgram *ps) {
    assert(("shader is not vertex shader", vs->GetType() == ShaderType::VERTEX_SHADER));
    assert(("shader is not pixel shader", ps->GetType() == ShaderType::PIXEL_SHADER));
    assert(("pipeline is not graphics pipeline", m_create_info.type == PipelineType::GRAPHICS));

    shader_map[ShaderType::VERTEX_SHADER] = vs;
    shader_map[ShaderType::PIXEL_SHADER] = ps;
    CreatePipelineResources();
}

void VulkanPipeline::CreateGraphicsPipeline() {

    auto ci = m_create_info.gpci;

    VkGraphicsPipelineCreateInfo graphics_pipeline_create_info{};
    graphics_pipeline_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    graphics_pipeline_create_info.flags = 0;
    graphics_pipeline_create_info.pNext = nullptr;

    // shader stage
    {
        std::vector<VkPipelineShaderStageCreateInfo> shader_stage_create_infos{};
        shader_stage_create_infos.reserve(shader_map.size());

        // no invalid shader in shader map
        for (auto &[type, shader] : shader_map) {
            auto sm = reinterpret_cast<VulkanShaderProgram *>(shader);
            VkPipelineShaderStageCreateInfo info{};
            info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            info.stage = ToVkShaderStageBit(sm->GetType());
            info.module = sm->m_shader_module;
            info.pName = "main";
            shader_stage_create_infos.push_back(std::move(info));
        }

        graphics_pipeline_create_info.stageCount = shader_stage_create_infos.size();
        graphics_pipeline_create_info.pStages = shader_stage_create_infos.data();
    }

    // vertex input state
    {

        uint32_t input_binding_count = 0;
        VkVertexInputBindingDescription input_bindings[VertexInputState::MAX_BINDING_COUNT] = {{0}};
        uint32_t input_attribute_count = 0;
        VkVertexInputAttributeDescription input_attributes[VertexInputState::MAX_ATTRIBUTE_COUNT] = {{0}};

        uint32_t binding_value = UINT32_MAX;

        // Initial values
        for (u32 i = 0; i < ci->vertex_input_state.attribute_count; ++i) {
            auto *attrib = &(ci->vertex_input_state.attributes[i]);

            if (binding_value != attrib->binding) {
                binding_value = attrib->binding;
                ++input_binding_count;
            }

            input_bindings[input_binding_count - 1].binding = binding_value;
            if (attrib->input_rate == VertexInputRate::VERTEX_ATTRIB_RATE_INSTANCE) {
                input_bindings[input_binding_count - 1].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
            } else {
                input_bindings[input_binding_count - 1].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            }
            input_bindings[input_binding_count - 1].stride +=
                GetStrideFromVertexAttributeDescription(attrib->attrib_format, attrib->portion);

            input_attributes[input_attribute_count].location = attrib->location;
            input_attributes[input_attribute_count].binding = attrib->binding;
            input_attributes[input_attribute_count].format = ToVkImageFormat(attrib->attrib_format, attrib->portion);
            input_attributes[input_attribute_count].offset = attrib->offset;
            ++input_attribute_count;
        }

        VkVertexInputBindingDescription bind_description{};

        VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info{};
        vertex_input_state_create_info.flags = 0;
        vertex_input_state_create_info.pNext = nullptr;
        vertex_input_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertex_input_state_create_info.vertexBindingDescriptionCount = input_binding_count;
        vertex_input_state_create_info.pVertexBindingDescriptions = input_bindings;
        vertex_input_state_create_info.vertexAttributeDescriptionCount = input_attribute_count;
        vertex_input_state_create_info.pVertexAttributeDescriptions = input_attributes;

        graphics_pipeline_create_info.pVertexInputState = &vertex_input_state_create_info;
    }

    // input assembly state
    {

        VkPipelineInputAssemblyStateCreateInfo input_assembly_state_create_info{};
        input_assembly_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_assembly_state_create_info.flags = 0;
        input_assembly_state_create_info.pNext = nullptr;
        input_assembly_state_create_info.topology = ToVkPrimitiveTopology(ci->input_assembly_state.topology);
        input_assembly_state_create_info.primitiveRestartEnable = VK_FALSE;

        graphics_pipeline_create_info.pInputAssemblyState = &input_assembly_state_create_info;
    }

    // tessllation
    { graphics_pipeline_create_info.pTessellationState = nullptr; }

    // viewport

    {
        VkPipelineViewportStateCreateInfo view_port_state_create_info{};
        view_port_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        view_port_state_create_info.flags = 0;
        view_port_state_create_info.pNext = nullptr;

        VkViewport view_port{};

        view_port.width = static_cast<f32>(ci->view_port_state.width);
        view_port.height = -static_cast<f32>(ci->view_port_state.height);
        view_port.x = 0.0f;
        view_port.y = -view_port.height;
        view_port.minDepth = 0.0f;
        view_port.maxDepth = 1.0f;

        VkExtent2D extent;
        extent.width = ci->view_port_state.width;
        extent.height = ci->view_port_state.height;

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = extent;

        VkPipelineViewportStateCreateInfo viewportStateCreateInfo{};
        viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportStateCreateInfo.viewportCount = 1;
        viewportStateCreateInfo.pViewports = &view_port;
        viewportStateCreateInfo.scissorCount = 1;
        viewportStateCreateInfo.pScissors = &scissor;

        graphics_pipeline_create_info.pViewportState = &view_port_state_create_info;
    }

    {
        VkPipelineRasterizationStateCreateInfo rasterization_state_create_info{};

        rasterization_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterization_state_create_info.depthClampEnable = VK_FALSE;
        rasterization_state_create_info.rasterizerDiscardEnable = VK_FALSE;
        rasterization_state_create_info.polygonMode = ToVkPolygonMode(ci->rasterization_state.fill_mode);
        rasterization_state_create_info.lineWidth = 1.0f;
        rasterization_state_create_info.cullMode = ToVkCullMode(ci->rasterization_state.cull_mode);
        rasterization_state_create_info.frontFace = ToVkFrontFace(ci->rasterization_state.front_face);
        rasterization_state_create_info.depthBiasEnable = VK_FALSE;

        graphics_pipeline_create_info.pRasterizationState = &rasterization_state_create_info;
    }

    // TODO: mulitsampling
    {
        VkPipelineMultisampleStateCreateInfo multi_sample_state_create_info{};

        multi_sample_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multi_sample_state_create_info.sampleShadingEnable = VK_FALSE;
        multi_sample_state_create_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        // multisamplingStateCreateInfo.minSampleShading = 1.0f; // Optional
        // multisamplingStateCreateInfo.pSampleMask = nullptr; // Optional
        // multisamplingStateCreateInfo.alphaToCoverageEnable = VK_FALSE; //
        // Optional multisamplingStateCreateInfo.alphaToOneEnable = VK_FALSE; //
        // Optional

        graphics_pipeline_create_info.pMultisampleState = &multi_sample_state_create_info;
    }

    // color blend
    {
        VkPipelineColorBlendStateCreateInfo color_blend_state_create_info{};
        color_blend_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;

        color_blend_state_create_info;
        graphics_pipeline_create_info.pColorBlendState = &color_blend_state_create_info;
    }

    // depth stencil
    {

        VkPipelineDepthStencilStateCreateInfo depth_stencil_state_create_info{};
        depth_stencil_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depth_stencil_state_create_info.depthTestEnable = ci->depth_stencil_state.depth_test;
        depth_stencil_state_create_info.depthWriteEnable = ci->depth_stencil_state.depth_write;
        depth_stencil_state_create_info.depthCompareOp = ToVkCompareOp(ci->depth_stencil_state.depth_func);
        depth_stencil_state_create_info.depthBoundsTestEnable = VK_FALSE;
        depth_stencil_state_create_info.stencilTestEnable = VK_FALSE;

        graphics_pipeline_create_info.pDepthStencilState = &depth_stencil_state_create_info;
    }

    // dyanmic state
    { graphics_pipeline_create_info.pDynamicState = nullptr; }

    graphics_pipeline_create_info.renderPass = 0;

    graphics_pipeline_create_info.layout = m_pipeline_layout;

    CHECK_VK_RESULT(
        vkCreateGraphicsPipelines(m_context.device, nullptr, 1, &graphics_pipeline_create_info, nullptr, &m_pipeline));
}

void VulkanPipeline::CreateComputePipeline() {
    if (!shader_map[ShaderType::COMPUTE_SHADER]) {
        LOG_ERROR("missing shader: compute shader");
        return;
    }
    auto cs = reinterpret_cast<VulkanShaderProgram *>(shader_map[ShaderType::COMPUTE_SHADER]);
    VkPipelineShaderStageCreateInfo shader_stage_create_info{};
    shader_stage_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stage_create_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    shader_stage_create_info.module = cs->m_shader_module;
    shader_stage_create_info.pName = "main";

    VkComputePipelineCreateInfo compute_pipeline_create_info{};

    // cache

    compute_pipeline_create_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    compute_pipeline_create_info.flags = 0;
    compute_pipeline_create_info.layout = m_pipeline_layout;
    compute_pipeline_create_info.stage = shader_stage_create_info;
    compute_pipeline_create_info.basePipelineHandle = nullptr;
    compute_pipeline_create_info.basePipelineIndex = 0;
    CHECK_VK_RESULT(
        vkCreateComputePipelines(m_context.device, nullptr, 1, &compute_pipeline_create_info, nullptr, &m_pipeline));
}
void VulkanPipeline::CreatePipelineLayout() {

    m_pipeline_layout_desc = m_descriptor_set_manager.CreateLayouts(shader_map, m_create_info.type);
    std::vector<VkDescriptorSetLayout> layouts;
    layouts.reserve(m_pipeline_layout_desc.descriptor_set_hash_key.size());
    for (u32 i = 0; i < m_pipeline_layout_desc.descriptor_set_hash_key.size(); i++) {
        auto key = m_pipeline_layout_desc.descriptor_set_hash_key[i];
        layouts.emplace_back(m_descriptor_set_manager.FindLayout(key));
    }

    VkPipelineLayoutCreateInfo pipeline_layout_create_info{};

    pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_create_info.setLayoutCount = static_cast<u32>(layouts.size());
    pipeline_layout_create_info.pSetLayouts = layouts.data();
    CHECK_VK_RESULT(
        vkCreatePipelineLayout(m_context.device, &pipeline_layout_create_info, nullptr, &m_pipeline_layout));
}
} // namespace Horizon::RHI