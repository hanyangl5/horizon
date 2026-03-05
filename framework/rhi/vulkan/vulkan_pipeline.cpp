#include "vulkan_pipeline.h"

#include <algorithm>
#include <rhi/vulkan/vulkan_shader.h>

namespace Horizon::Backend
{

VulkanPipeline::VulkanPipeline(const VulkanRendererContext &context, const GraphicsPipelineCreateInfo &create_info,
                               VulkanDescriptorSetAllocator &descriptor_set_manager) noexcept
    : m_context(context), m_descriptor_set_allocator(descriptor_set_manager)
{
    m_type = PipelineType::GRAPHICS;
    CreatePipelineLayout(create_info.shader_program);
    CreateGraphicsPipeline(create_info);
}

VulkanPipeline::VulkanPipeline(const VulkanRendererContext &context,
                               [[maybe_unused]] const ComputePipelineCreateInfo &create_info,
                               VulkanDescriptorSetAllocator &descriptor_set_manager) noexcept
    : m_context(context), m_descriptor_set_allocator(descriptor_set_manager)
{
    m_type = PipelineType::COMPUTE;
    CreatePipelineLayout(create_info.shader_program);
    CreateComputePipeline(create_info);
    // m_create_info.cpci = const_cast<ComputePipelineCreateInfo *>(std::move(&create_info));
}

VulkanPipeline::~VulkanPipeline() noexcept
{
    m_descriptor_set_allocator.ReleaseDescriptorSets(this);
    vkDestroyPipeline(m_context.device, m_pipeline, nullptr);
    vkDestroyPipelineLayout(m_context.device, m_pipeline_layout, nullptr);
}
// void VulkanPipeline::SetComputeShader(Shader *cs)
//{
//    assert(cs->GetType() == ShaderType::COMPUTE_SHADER);
//    assert(m_create_info.type == PipelineType::COMPUTE);
//
//    if (m_cs == nullptr)
//    {
//        m_cs = cs;
//        CreatePipelineLayout();
//        CreateComputePipeline();
//    }
//}
//
// void VulkanPipeline::SetGraphicsShader(Shader *vs, Shader *ps)
//{
//    assert(vs->GetType() == ShaderType::VERTEX_SHADER);
//    assert(ps->GetType() == ShaderType::PIXEL_SHADER);
//    assert(m_create_info.type == PipelineType::GRAPHICS);
//
//    if (m_vs == nullptr && m_ps == nullptr)
//    {
//        m_vs = vs;
//        m_ps = ps;
//        CreatePipelineLayout();
//        CreateGraphicsPipeline();
//    }
//}

void VulkanPipeline::SetResource(Buffer *resource, const std::string &resource_name)
{
    auto ds = m_descriptor_set_allocator.GetDescriptorSet(this);
    if (ds == nullptr)
    {
        LOG_ERROR("Failed to get descriptor set0 while setting buffer resource '{}'", resource_name);
        return;
    }
    ds->SetResource(resource, resource_name);
}
void VulkanPipeline::SetResource(Texture *resource, const std::string &resource_name)
{
    auto ds = m_descriptor_set_allocator.GetDescriptorSet(this);
    if (ds == nullptr)
    {
        LOG_ERROR("Failed to get descriptor set0 while setting texture resource '{}'", resource_name);
        return;
    }
    ds->SetResource(resource, resource_name);
}
void VulkanPipeline::SetResource(Sampler *resource, const std::string &resource_name)
{
    auto ds = m_descriptor_set_allocator.GetDescriptorSet(this);
    if (ds == nullptr)
    {
        LOG_ERROR("Failed to get descriptor set0 while setting sampler resource '{}'", resource_name);
        return;
    }
    ds->SetResource(resource, resource_name);
}
void VulkanPipeline::SetBindlessResource(std::vector<Buffer *> &resource, const std::string &resource_name)
{
    auto ds = m_descriptor_set_allocator.GetBindlessDescriptorSet(this);
    if (ds == nullptr)
    {
        LOG_ERROR("Failed to get descriptor set1 while setting bindless buffer resource '{}'", resource_name);
        return;
    }
    ds->SetBindlessResource(resource, resource_name);
}
void VulkanPipeline::SetBindlessResource(std::vector<Texture *> &resource, const std::string &resource_name)
{
    auto ds = m_descriptor_set_allocator.GetBindlessDescriptorSet(this);
    if (ds == nullptr)
    {
        LOG_ERROR("Failed to get descriptor set1 while setting bindless texture resource '{}'", resource_name);
        return;
    }
    ds->SetBindlessResource(resource, resource_name);
}

VulkanDescriptorSet *VulkanPipeline::GetDescriptorSet()
{
    return m_descriptor_set_allocator.GetDescriptorSet(this);
}
VulkanDescriptorSet *VulkanPipeline::GetBindlessDescriptorSet()
{
    return m_descriptor_set_allocator.GetBindlessDescriptorSet(this);
}
void VulkanPipeline::CreateGraphicsPipeline(const GraphicsPipelineCreateInfo &create_info)
{
    // auto ci = m_create_info.gpci;
    {
        const bool uses_mesh_shading = (create_info.shader_program.MeshShader() != nullptr);

        VkGraphicsPipelineCreateInfo graphics_pipeline_create_info{};
        graphics_pipeline_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        graphics_pipeline_create_info.flags = 0;
        graphics_pipeline_create_info.pNext = nullptr;

        uint32_t input_binding_count = 0;
        std::array<VkVertexInputBindingDescription, MAX_BINDING_COUNT> input_bindings = {};
        uint32_t input_attribute_count = 0;
        std::array<VkVertexInputAttributeDescription, MAX_ATTRIBUTE_COUNT> input_attributes = {};
        std::vector<VkPipelineShaderStageCreateInfo> shader_stage_create_infos;
        VkPipelineRasterizationStateCreateInfo rasterization_state_create_info{};
        VkPipelineMultisampleStateCreateInfo multi_sample_state_create_info{};
        VkPipelineColorBlendStateCreateInfo color_blend_state_create_info{};
        VkPipelineDepthStencilStateCreateInfo depth_stencil_state_create_info{};
        VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info{};
        VkPipelineInputAssemblyStateCreateInfo input_assembly_state_create_info{};
        VkPipelineViewportStateCreateInfo view_port_state_create_info{};
        VkPipelineRenderingCreateInfo rendering_create_info{};
        VkPipelineDynamicStateCreateInfo dynamic_state_create_info{};
        std::array<VkDynamicState, 2> dynamic_states{};
        std::vector<VkPipelineColorBlendAttachmentState> color_blend_attachment_state;
        // shader stage
        {
            shader_stage_create_infos.reserve(3);

            if (uses_mesh_shading)
            {
                if (auto ts = reinterpret_cast<VulkanShader *>(create_info.shader_program.TaskShader()))
                {
                    shader_stage_create_infos.emplace_back(VkPipelineShaderStageCreateInfo{
                        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
                        ToVkShaderStageBit(ts->GetType()), ts->m_shader_module, ts->GetEntryPoint(), nullptr});
                }

                auto ms = reinterpret_cast<VulkanShader *>(create_info.shader_program.MeshShader());
                if (ms == nullptr)
                {
                    LOG_ERROR("Mesh shading pipeline requires a mesh shader.");
                    return;
                }
                shader_stage_create_infos.emplace_back(VkPipelineShaderStageCreateInfo{
                    VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, ToVkShaderStageBit(ms->GetType()),
                    ms->m_shader_module, ms->GetEntryPoint(), nullptr});
            }
            else
            {
                auto vs = reinterpret_cast<VulkanShader *>(create_info.shader_program.VertexShader());
                if (vs == nullptr)
                {
                    LOG_ERROR("Graphics pipeline requires a vertex shader when mesh shader is not used.");
                    return;
                }

                shader_stage_create_infos.emplace_back(VkPipelineShaderStageCreateInfo{
                    VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, ToVkShaderStageBit(vs->GetType()),
                    vs->m_shader_module, vs->GetEntryPoint(), nullptr});
            }

            auto ps = reinterpret_cast<VulkanShader *>(create_info.shader_program.PixelShader());
            if (ps != nullptr)
            {
                shader_stage_create_infos.emplace_back(VkPipelineShaderStageCreateInfo{
                    VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, ToVkShaderStageBit(ps->GetType()),
                    ps->m_shader_module, ps->GetEntryPoint(), nullptr});
            }

            graphics_pipeline_create_info.stageCount = (u32)shader_stage_create_infos.size();
            graphics_pipeline_create_info.pStages = shader_stage_create_infos.data();
        }

        // vertex input state
        {
            if (!uses_mesh_shading)
            {
                for (u32 i = 0; i < create_info.vertex_input_state.attribute_count; ++i)
                {
                    auto *attrib = &(create_info.vertex_input_state.attributes[i]);
                    uint32_t binding_index = UINT32_MAX;
                    for (uint32_t j = 0; j < input_binding_count; ++j)
                    {
                        if (input_bindings[j].binding == attrib->binding)
                        {
                            binding_index = j;
                            break;
                        }
                    }
                    if (binding_index == UINT32_MAX)
                    {
                        binding_index = input_binding_count++;
                        input_bindings[binding_index].binding = attrib->binding;
                        input_bindings[binding_index].stride = 0;
                        input_bindings[binding_index].inputRate =
                            (attrib->input_rate == VertexInputRate::VERTEX_ATTRIB_RATE_INSTANCE)
                                ? VK_VERTEX_INPUT_RATE_INSTANCE
                                : VK_VERTEX_INPUT_RATE_VERTEX;
                    }
                    else
                    {
                        const VkVertexInputRate expected_rate =
                            (attrib->input_rate == VertexInputRate::VERTEX_ATTRIB_RATE_INSTANCE)
                                ? VK_VERTEX_INPUT_RATE_INSTANCE
                                : VK_VERTEX_INPUT_RATE_VERTEX;
                        if (input_bindings[binding_index].inputRate != expected_rate)
                        {
                            LOG_WARN("Vertex input binding {} has mixed input rates; using first declared rate",
                                     attrib->binding);
                        }
                    }

                    const uint32_t attrib_stride =
                        (attrib->stride != 0)
                            ? attrib->stride
                            : GetStrideFromVertexAttributeDescription(attrib->attrib_format, attrib->portion);
                    input_bindings[binding_index].stride =
                        std::max(input_bindings[binding_index].stride, attrib_stride);

                    input_attributes[input_attribute_count].location = attrib->location;
                    input_attributes[input_attribute_count].binding = attrib->binding;
                    input_attributes[input_attribute_count].format =
                        ToVkImageFormat(attrib->attrib_format, attrib->portion);
                    input_attributes[input_attribute_count].offset = attrib->offset;
                    ++input_attribute_count;
                }
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
            input_assembly_state_create_info.topology =
                uses_mesh_shading ? VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
                                  : ToVkPrimitiveTopology(create_info.input_assembly_state.topology);
            input_assembly_state_create_info.primitiveRestartEnable = VK_FALSE;

            graphics_pipeline_create_info.pInputAssemblyState = &input_assembly_state_create_info;
        }

        // tessllation state
        {
            graphics_pipeline_create_info.pTessellationState = nullptr;
        }

        // viewport state

        {
            view_port_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
            view_port_state_create_info.flags = 0;
            view_port_state_create_info.pNext = nullptr;

            view_port.width = static_cast<f32>(create_info.view_port_state.width);
            view_port.height = -static_cast<f32>(create_info.view_port_state.height);
            view_port.x = 0.0f;
            view_port.y = -view_port.height;
            view_port.minDepth = 0.0f;
            view_port.maxDepth = 1.0f;

            VkExtent2D extent;
            extent.width = create_info.view_port_state.width;
            extent.height = create_info.view_port_state.height;

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
            rasterization_state_create_info.polygonMode = ToVkPolygonMode(create_info.rasterization_state.fill_mode);
            rasterization_state_create_info.lineWidth = 1.0f;
            rasterization_state_create_info.cullMode = ToVkCullMode(create_info.rasterization_state.cull_mode);
            rasterization_state_create_info.frontFace = ToVkFrontFace(create_info.rasterization_state.front_face);
            rasterization_state_create_info.depthBiasEnable = VK_FALSE;

            graphics_pipeline_create_info.pRasterizationState = &rasterization_state_create_info;
        }

        // TODO(hylu): mulitsampling
        {

            multi_sample_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
            multi_sample_state_create_info.sampleShadingEnable = VK_FALSE;
            multi_sample_state_create_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

            graphics_pipeline_create_info.pMultisampleState = &multi_sample_state_create_info;
        }

        // color blend state
        {
            color_blend_attachment_state.resize(
                create_info.render_target_formats.color_attachment_count); // TODO(hylu): reserve and construct
            for (auto &state : color_blend_attachment_state)
            {
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
            depth_stencil_state_create_info.depthTestEnable = create_info.depth_stencil_state.depth_test;
            depth_stencil_state_create_info.depthWriteEnable = create_info.depth_stencil_state.depth_write;
            depth_stencil_state_create_info.depthCompareOp = ToVkCompareOp(create_info.depth_stencil_state.depth_func);
            depth_stencil_state_create_info.depthBoundsTestEnable = VK_FALSE;
            depth_stencil_state_create_info.stencilTestEnable = VK_FALSE;

            graphics_pipeline_create_info.pDepthStencilState = &depth_stencil_state_create_info;
        }

        // dyanmic state
        {
            dynamic_states = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
            dynamic_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
            dynamic_state_create_info.dynamicStateCount = static_cast<u32>(dynamic_states.size());
            dynamic_state_create_info.pDynamicStates = dynamic_states.data();
            graphics_pipeline_create_info.pDynamicState = &dynamic_state_create_info;
        }

        rendering_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
        rendering_create_info.colorAttachmentCount = create_info.render_target_formats.color_attachment_count;

        std::vector<VkFormat> formats(create_info.render_target_formats.color_attachment_count);
        for (u32 i = 0; i < create_info.render_target_formats.color_attachment_count; i++)
        {
            formats[i] = ToVkImageFormat(create_info.render_target_formats.color_attachment_formats[i]);
        }

        rendering_create_info.pColorAttachmentFormats = formats.data();
        if (create_info.render_target_formats.has_depth)
            rendering_create_info.depthAttachmentFormat =
                ToVkImageFormat(create_info.render_target_formats.depth_stencil_format);
        if (create_info.render_target_formats.has_stencil)
            rendering_create_info.stencilAttachmentFormat =
                ToVkImageFormat(create_info.render_target_formats.depth_stencil_format);

        graphics_pipeline_create_info.pNext = &rendering_create_info;

        graphics_pipeline_create_info.layout = m_pipeline_layout;

        CHECK_VK_RESULT(vkCreateGraphicsPipelines(m_context.device, nullptr, 1, &graphics_pipeline_create_info, nullptr,
                                                  &m_pipeline));
    }
}

void VulkanPipeline::CreateComputePipeline(const ComputePipelineCreateInfo &create_info)
{
    // assert(m_cs != nullptr);

    auto cs = reinterpret_cast<VulkanShader *>(create_info.shader_program.ComputeShader());
    VkPipelineShaderStageCreateInfo shader_stage_create_info{};
    shader_stage_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stage_create_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    shader_stage_create_info.module = cs->m_shader_module;
    shader_stage_create_info.pName = cs->GetEntryPoint();

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

void VulkanPipeline::CreatePipelineLayout(const ShaderPrograms &shaders)
{

    VkPipelineLayoutCreateInfo pipeline_layout_create_info{};

    pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    ParseRootSignature(shaders);

    // if no descriptor declared in shader
    bool need_descriptorset = !rsd.descriptors.empty();

    std::vector<VkDescriptorSetLayout> layouts;
    std::vector<VkPushConstantRange> push_constant_ranges;

    if (need_descriptorset)
    {

        m_descriptor_set_allocator.CreateDescriptorSetLayout(this);

        const VkDescriptorSetLayout empty_layout = m_descriptor_set_allocator.GetVkDescriptorSetLayout(
            m_descriptor_set_allocator.m_empty_descriptor_set_layout_hash_key);
        const bool has_bindless_set = (m_pipeline_layout_desc.bindless_descriptor_set_hash_key != 0);
        const bool has_default_set = (m_pipeline_layout_desc.descriptor_set_hash_key != 0);

        if (has_bindless_set)
        {
            layouts.resize(BINDLESS_DESCRIPTOR_SET_NUMBER + 1, empty_layout);
            layouts[DEFAULT_DESCRIPTOR_SET_NUMBER] = has_default_set
                                                         ? m_descriptor_set_allocator.GetVkDescriptorSetLayout(
                                                               m_pipeline_layout_desc.descriptor_set_hash_key)
                                                         : empty_layout;
            layouts[BINDLESS_DESCRIPTOR_SET_NUMBER] = m_descriptor_set_allocator.GetVkDescriptorSetLayout(
                m_pipeline_layout_desc.bindless_descriptor_set_hash_key);
        }
        else if (has_default_set)
        {
            layouts.resize(DEFAULT_DESCRIPTOR_SET_NUMBER + 1, empty_layout);
            layouts[DEFAULT_DESCRIPTOR_SET_NUMBER] =
                m_descriptor_set_allocator.GetVkDescriptorSetLayout(m_pipeline_layout_desc.descriptor_set_hash_key);
        }

        push_constant_ranges.reserve(rsd.push_constants.size());

        for (auto &[name, pc] : rsd.push_constants)
        {
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
    }
    else
    {
        pipeline_layout_create_info.setLayoutCount = 0;
        pipeline_layout_create_info.pSetLayouts = nullptr;
        pipeline_layout_create_info.pushConstantRangeCount = 0;
        pipeline_layout_create_info.pPushConstantRanges = nullptr;
    }

    CHECK_VK_RESULT(
        vkCreatePipelineLayout(m_context.device, &pipeline_layout_create_info, nullptr, &m_pipeline_layout));
}

} // namespace Horizon::Backend
