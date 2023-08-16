#include "VulkanPipeline.h"

#include <runtime/rhi/vulkan/VulkanShader.h>

namespace Horizon::Backend {

VulkanPipeline::VulkanPipeline(const VulkanRendererContext &context, const GraphicsPipelineCreateInfo &create_info,
                               VulkanDescriptorSetAllocator &descriptor_set_manager) noexcept
    : m_context(context), m_descriptor_set_allocator(descriptor_set_manager) {
    m_create_info.type = PipelineType::GRAPHICS;
    m_create_info.gpci = const_cast<GraphicsPipelineCreateInfo *>(std::move(&create_info));
}

VulkanPipeline::VulkanPipeline(const VulkanRendererContext &context,
                               [[maybe_unused]] const ComputePipelineCreateInfo &create_info,
                               VulkanDescriptorSetAllocator &descriptor_set_manager) noexcept
    : m_context(context), m_descriptor_set_allocator(descriptor_set_manager) {
    m_create_info.type = PipelineType::COMPUTE;
    m_create_info.cpci = const_cast<ComputePipelineCreateInfo *>(std::move(&create_info));
}

VulkanPipeline::~VulkanPipeline() noexcept {
    vkDestroyPipeline(m_context.device, m_pipeline, nullptr);
    vkDestroyPipelineLayout(m_context.device, m_pipeline_layout, nullptr);
}
void VulkanPipeline::SetComputeShader(Shader *cs) {
    assert(("shader is not compute shader", cs->GetType() == ShaderType::COMPUTE_SHADER));
    assert(("pipeline is not compute shader", m_create_info.type == PipelineType::COMPUTE));

    if (m_cs == nullptr) {
        m_cs = cs;
        CreatePipelineLayout();
        CreateComputePipeline();
    }

    // m_descriptor_set_allocator.UpdateDescriptorPoolInfo(this, m_pipeline_layout_desc.descriptor_set_hash_key);
}

void VulkanPipeline::SetGraphicsShader(Shader *vs, Shader *ps) {
    assert(("shader is not vertex shader", vs->GetType() == ShaderType::VERTEX_SHADER));
    assert(("shader is not pixel shader", ps->GetType() == ShaderType::PIXEL_SHADER));
    assert(("pipeline is not graphics pipeline", m_create_info.type == PipelineType::GRAPHICS));

    if (m_vs == nullptr && m_ps == nullptr) {
        m_vs = vs;
        m_ps = ps;
        CreatePipelineLayout();
        CreateGraphicsPipeline();
    }
}

DescriptorSet *VulkanPipeline::GetDescriptorSet(ResourceUpdateFrequency frequency) {

    if (frequency == ResourceUpdateFrequency::BINDLESS) {

        if (!m_descriptor_set_allocator.m_bindless_descriptor_pool) {
            m_descriptor_set_allocator.CreateBindlessDescriptorPool();
        }

        VkDescriptorSetAllocateInfo alloc_info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        alloc_info.descriptorPool = m_descriptor_set_allocator.m_bindless_descriptor_pool;

        alloc_info.descriptorSetCount = 1;
        VkDescriptorSetLayout layout = m_descriptor_set_allocator.GetVkDescriptorSetLayout(
            this->m_pipeline_layout_desc.descriptor_set_hash_key[static_cast<u32>(frequency)]);
        alloc_info.pSetLayouts = &layout;

        VkDescriptorSetVariableDescriptorCountAllocateInfo count_info{
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO_EXT};
        u32 max_binding = m_descriptor_set_allocator.k_max_bindless_resources;
        count_info.descriptorSetCount = 1;
        // This number is the max allocatable count
        count_info.pDescriptorCounts = &max_binding;
        alloc_info.pNext = &count_info;
        VkDescriptorSet vk_ds;
        CHECK_VK_RESULT(vkAllocateDescriptorSets(m_context.device, &alloc_info, &vk_ds));
        DescriptorSet *set =
            new VulkanDescriptorSet(m_context, frequency, rsd.descriptors[static_cast<u32>(frequency)], vk_ds);
        m_descriptor_set_allocator.allocated_sets.push_back(set);
        return set;
    } else {

        if (!m_descriptor_set_allocator.m_temp_descriptor_pool) {
            m_descriptor_set_allocator.CreateDescriptorPool();
        }

        VkDescriptorSetAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;

        VkDescriptorSetLayout layout = m_descriptor_set_allocator.GetVkDescriptorSetLayout(
            this->m_pipeline_layout_desc.descriptor_set_hash_key[static_cast<u32>(frequency)]);

        alloc_info.descriptorPool = m_descriptor_set_allocator.m_temp_descriptor_pool;
        alloc_info.descriptorSetCount = 1;
        alloc_info.pSetLayouts = &layout;
        VkDescriptorSet vk_ds;
        CHECK_VK_RESULT(vkAllocateDescriptorSets(m_context.device, &alloc_info, &vk_ds));
        DescriptorSet *set =
            new VulkanDescriptorSet(m_context, frequency, rsd.descriptors[static_cast<u32>(frequency)], vk_ds);
        m_descriptor_set_allocator.allocated_sets.push_back(set);
        return set;
    }
}
void VulkanPipeline::CreateGraphicsPipeline() {
    auto ci = m_create_info.gpci;
    {

        VkGraphicsPipelineCreateInfo graphics_pipeline_create_info{};
        graphics_pipeline_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        graphics_pipeline_create_info.flags = 0;
        graphics_pipeline_create_info.pNext = nullptr;

        uint32_t input_binding_count = 0;
        std::array<VkVertexInputBindingDescription, MAX_BINDING_COUNT> input_bindings = {};
        uint32_t input_attribute_count = 0;
        std::array<VkVertexInputAttributeDescription, MAX_ATTRIBUTE_COUNT> input_attributes = {};
        std::vector<VkPipelineShaderStageCreateInfo> shader_stage_create_infos;
        LOG_INFO("{}", sizeof(VkPipelineShaderStageCreateInfo));
        VkPipelineRasterizationStateCreateInfo rasterization_state_create_info{};
        VkPipelineMultisampleStateCreateInfo multi_sample_state_create_info{};
        VkPipelineColorBlendStateCreateInfo color_blend_state_create_info{};
        VkPipelineDepthStencilStateCreateInfo depth_stencil_state_create_info{};
        VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info{};
        VkPipelineInputAssemblyStateCreateInfo input_assembly_state_create_info{};
        VkPipelineViewportStateCreateInfo view_port_state_create_info{};
        VkPipelineRenderingCreateInfo rendering_create_info{};
        std::vector<VkPipelineColorBlendAttachmentState> color_blend_attachment_state;
        // shader stage
        {

            shader_stage_create_infos.reserve(2);
            {
                auto vs = reinterpret_cast<VulkanShader *>(m_vs);

                shader_stage_create_infos.emplace_back(VkPipelineShaderStageCreateInfo{
                    VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, ToVkShaderStageBit(vs->GetType()),
                    vs->m_shader_module, "main", nullptr});

                auto ps = reinterpret_cast<VulkanShader *>(m_ps);

                shader_stage_create_infos.emplace_back(VkPipelineShaderStageCreateInfo{
                    VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, ToVkShaderStageBit(ps->GetType()),
                    ps->m_shader_module, "main", nullptr});
            }

            graphics_pipeline_create_info.stageCount = (u32)shader_stage_create_infos.size();
            graphics_pipeline_create_info.pStages = shader_stage_create_infos.data();
        }

        // vertex input state
        {

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
                input_attributes[input_attribute_count].format =
                    ToVkImageFormat(attrib->attrib_format, attrib->portion);
                input_attributes[input_attribute_count].offset = attrib->offset;
                ++input_attribute_count;
            }

            vertex_input_state_create_info.flags = 0;
            vertex_input_state_create_info.pNext = nullptr;
            vertex_input_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
            vertex_input_state_create_info.vertexBindingDescriptionCount = input_binding_count;
            vertex_input_state_create_info.pVertexBindingDescriptions = input_bindings.data();
            vertex_input_state_create_info.vertexAttributeDescriptionCount = input_attribute_count;
            vertex_input_state_create_info.pVertexAttributeDescriptions = input_attributes.data();

            graphics_pipeline_create_info.pVertexInputState = &vertex_input_state_create_info;
        }

        // input assembly state
        {

            input_assembly_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
            input_assembly_state_create_info.flags = 0;
            input_assembly_state_create_info.pNext = nullptr;
            input_assembly_state_create_info.topology = ToVkPrimitiveTopology(ci->input_assembly_state.topology);
            input_assembly_state_create_info.primitiveRestartEnable = VK_FALSE;

            graphics_pipeline_create_info.pInputAssemblyState = &input_assembly_state_create_info;
        }

        // tessllation state
        { graphics_pipeline_create_info.pTessellationState = nullptr; }

        // viewport state

        {
            view_port_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
            view_port_state_create_info.flags = 0;
            view_port_state_create_info.pNext = nullptr;

            view_port.width = static_cast<f32>(ci->view_port_state.width);
            view_port.height = -static_cast<f32>(ci->view_port_state.height);
            view_port.x = 0.0f;
            view_port.y = -view_port.height;
            view_port.minDepth = 0.0f;
            view_port.maxDepth = 1.0f;

            VkExtent2D extent;
            extent.width = ci->view_port_state.width;
            extent.height = ci->view_port_state.height;

            scissor.offset = {0, 0};
            scissor.extent = extent;

            view_port_state_create_info.viewportCount = 1;
            view_port_state_create_info.pViewports = &view_port;
            view_port_state_create_info.scissorCount = 1;
            view_port_state_create_info.pScissors = &scissor;

            graphics_pipeline_create_info.pViewportState = &view_port_state_create_info;
        }

        // rasterization state
        {

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

        // TODO(hylu): mulitsampling
        {

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

        // color blend state
        {
            color_blend_attachment_state.resize(
                ci->render_target_formats.color_attachment_count); // TODO(hylu): reserve and construct
            for (auto &state : color_blend_attachment_state) {
                state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                                       VK_COLOR_COMPONENT_A_BIT;
                state.blendEnable = VK_FALSE;
            }

            color_blend_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            color_blend_state_create_info.logicOpEnable = VK_FALSE;
            color_blend_state_create_info.logicOp = VK_LOGIC_OP_COPY;
            color_blend_state_create_info.attachmentCount = static_cast<u32>(color_blend_attachment_state.size());
            color_blend_state_create_info.pAttachments = color_blend_attachment_state.data();
            color_blend_state_create_info.blendConstants[0] = 0.0f;
            color_blend_state_create_info.blendConstants[1] = 0.0f;
            color_blend_state_create_info.blendConstants[2] = 0.0f;
            color_blend_state_create_info.blendConstants[3] = 0.0f;
            graphics_pipeline_create_info.pColorBlendState = &color_blend_state_create_info;
        }

        // depth stencil
        {

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

        rendering_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
        rendering_create_info.colorAttachmentCount = ci->render_target_formats.color_attachment_count;

        std::vector<VkFormat> formats(ci->render_target_formats.color_attachment_count);
        for (u32 i = 0; i < ci->render_target_formats.color_attachment_count; i++) {
            formats[i] = ToVkImageFormat(ci->render_target_formats.color_attachment_formats[i]);
        }

        rendering_create_info.pColorAttachmentFormats = formats.data();
        if (ci->render_target_formats.has_depth)
            rendering_create_info.depthAttachmentFormat =
                ToVkImageFormat(ci->render_target_formats.depth_stencil_format);
        if (ci->render_target_formats.has_stencil)
            rendering_create_info.stencilAttachmentFormat =
                ToVkImageFormat(ci->render_target_formats.depth_stencil_format);

        graphics_pipeline_create_info.pNext = &rendering_create_info;
        // graphics_pipeline_create_info.renderPass = VK_NULL_HANDLE;

        graphics_pipeline_create_info.layout = m_pipeline_layout;

        CHECK_VK_RESULT(vkCreateGraphicsPipelines(m_context.device, nullptr, 1, &graphics_pipeline_create_info, nullptr,
                                                  &m_pipeline));
    }
}

void VulkanPipeline::CreateComputePipeline() {
    assert(("shader not exist", m_cs != nullptr));

    auto cs = reinterpret_cast<VulkanShader *>(m_cs);
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

    VkPipelineLayoutCreateInfo pipeline_layout_create_info{};

    pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    ParseRootSignature();

    // if no descriptor declared in shader
    bool need_descriptorset = !rsd.descriptors.empty();

    std::vector<VkDescriptorSetLayout> layouts;
    std::vector<VkPushConstantRange> push_constant_ranges;

    if (need_descriptorset) {

        m_descriptor_set_allocator.CreateDescriptorSetLayout(this);

        layouts.reserve(m_pipeline_layout_desc.descriptor_set_hash_key.size());
        for (auto &key : m_pipeline_layout_desc.descriptor_set_hash_key) {

            layouts.emplace_back(m_descriptor_set_allocator.GetVkDescriptorSetLayout(key));
        }

        push_constant_ranges.reserve(rsd.push_constants.size());

        for (auto &[name, pc] : rsd.push_constants) {
            push_constant_ranges.emplace_back(VkPushConstantRange{
                ToVkShaderStageFlags(pc.shader_stages),
                pc.offset,
                pc.size,
            });
        }

        pipeline_layout_create_info.setLayoutCount = static_cast<u32>(layouts.size());
        pipeline_layout_create_info.pSetLayouts = layouts.data();
        pipeline_layout_create_info.pushConstantRangeCount = static_cast<u32>(push_constant_ranges.size());
        pipeline_layout_create_info.pPushConstantRanges = push_constant_ranges.data();
    } else {
        pipeline_layout_create_info.setLayoutCount = 0;
        pipeline_layout_create_info.pSetLayouts = nullptr;
        pipeline_layout_create_info.pushConstantRangeCount = 0;
        pipeline_layout_create_info.pPushConstantRanges = nullptr;
    }

    CHECK_VK_RESULT(
        vkCreatePipelineLayout(m_context.device, &pipeline_layout_create_info, nullptr, &m_pipeline_layout));
}

} // namespace Horizon::Backend