#include "Pipeline.h"

#include <array>

#include <runtime/core/log/Log.h>

#include "Device.h"
#include "Instance.h"
#include "ShaderModule.h"
#include "Surface.h"
#include "SwapChain.h"
#include "Vertex.h"

namespace Horizon {
Pipeline::Pipeline(std::shared_ptr<Device> device) noexcept : m_device(device) {}

Pipeline::~Pipeline() noexcept {
    vkDestroyPipeline(m_device->Get(), m_pipeline, nullptr);
    vkDestroyPipelineLayout(m_device->Get(), m_pipeline_layout, nullptr);
}

VkPipeline Pipeline::Get() const noexcept { return m_pipeline; }

VkPipelineLayout Pipeline::GetLayout() const noexcept { return m_pipeline_layout; }

bool Pipeline::hasPushConstants() const noexcept { return m_push_constants != nullptr; }

PipelineType Pipeline::GetType() const noexcept { return m_type; }

GraphicsPipeline::GraphicsPipeline(std::shared_ptr<Device> device, const GraphicsPipelineCreateInfo &create_info,
                                   const std::vector<AttachmentCreateInfo> &attachment_create_info,
                                   const RenderContext render_context, std::shared_ptr<SwapChain> swap_chain) noexcept
    : Pipeline(device), m_render_context(render_context) {
    m_type = PipelineType::GRAPHICS;

    if (swap_chain) {
        m_framebuffer = std::make_shared<Framebuffer>(m_device, attachment_create_info, m_render_context, swap_chain);
    } else {
        m_framebuffer = std::make_shared<Framebuffer>(m_device, attachment_create_info, m_render_context);
    }
    CreatePipelineLayout(create_info);
    CreatePipeline(create_info);
    m_clear_values = m_framebuffer->getClearValues();
}

GraphicsPipeline::~GraphicsPipeline() noexcept {}

VkViewport GraphicsPipeline::getViewport() const noexcept { return m_viewport; }

VkRenderPass GraphicsPipeline::getRenderPass() const noexcept { return m_framebuffer->getRenderPass(); }

VkFramebuffer GraphicsPipeline::getFrameBuffer() const noexcept { return m_framebuffer->Get(); }

VkFramebuffer GraphicsPipeline::getFrameBuffer(u32 index) const noexcept { return m_framebuffer->Get(index); }

std::shared_ptr<AttachmentDescriptor> GraphicsPipeline::GetFrameBufferAttachment(u32 attachment_index) const noexcept {
    return m_framebuffer->getDescriptorImageInfo(attachment_index);
}

std::vector<VkImage> GraphicsPipeline::getPresentImages() const noexcept { return std::vector<VkImage>(); }

std::vector<VkClearValue> GraphicsPipeline::getClearValues() const noexcept { return m_clear_values; }

void GraphicsPipeline::CreatePipelineLayout(const GraphicsPipelineCreateInfo &create_info) {
    //auto &layouts = create_info.descriptor_layouts;
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    // prevent crash if no descriptors are used in shader
    if (create_info.descriptor_layouts) {
        pipelineLayoutInfo.setLayoutCount = static_cast<u32>(create_info.descriptor_layouts->layouts.size());
        pipelineLayoutInfo.pSetLayouts = create_info.descriptor_layouts->layouts.data();
    } else {
        pipelineLayoutInfo.setLayoutCount = 0;
        pipelineLayoutInfo.pSetLayouts = nullptr;
    }

    std::vector<VkPushConstantRange> push_constant_ranges;

    if (create_info.push_constants) {
        push_constant_ranges.resize(create_info.push_constants->ranges.size());
        m_push_constants = create_info.push_constants;

        for (u32 i = 0; i < m_push_constants->ranges.size(); i++) {
            push_constant_ranges[i].stageFlags =
                ToVkShaderStageFlags(m_push_constants->ranges[i].stages); // convert to vulkan shader stage flags
            push_constant_ranges[i].offset = m_push_constants->ranges[i].offset;
            push_constant_ranges[i].size = m_push_constants->ranges[i].size;
        }
        pipelineLayoutInfo.pushConstantRangeCount = push_constant_ranges.size();
        pipelineLayoutInfo.pPushConstantRanges = push_constant_ranges.data();
    }
    CHECK_VK_RESULT(vkCreatePipelineLayout(m_device->Get(), &pipelineLayoutInfo, nullptr, &m_pipeline_layout));
}

void GraphicsPipeline::CreatePipeline(const GraphicsPipelineCreateInfo &create_info) {

    std::array<VkPipelineShaderStageCreateInfo, 2> pipelineShaderStageCreateInfos{};

    // vertex shader
    pipelineShaderStageCreateInfos[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineShaderStageCreateInfos[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    pipelineShaderStageCreateInfos[0].module = create_info.vs->Get();
    pipelineShaderStageCreateInfos[0].pName = "main";

    // pixel shader
    pipelineShaderStageCreateInfos[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineShaderStageCreateInfos[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    pipelineShaderStageCreateInfos[1].module = create_info.ps->Get();
    pipelineShaderStageCreateInfos[1].pName = "main";

    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo{};
    vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputStateCreateInfo.vertexBindingDescriptionCount = 1;
    vertexInputStateCreateInfo.pVertexBindingDescriptions = &bindingDescription; // Optional
    vertexInputStateCreateInfo.vertexAttributeDescriptionCount = static_cast<u32>(attributeDescriptions.size());
    ;
    vertexInputStateCreateInfo.pVertexAttributeDescriptions = attributeDescriptions.data(); // Optional

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo{};
    inputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;

    // A viewport basically describes the region of the framebuffer that the output will be rendered to
    m_viewport.width = static_cast<f32>(m_render_context.width);
    m_viewport.height = -static_cast<f32>(m_render_context.height);
    m_viewport.x = 0.0f;
    m_viewport.y = -m_viewport.height;
    m_viewport.minDepth = 0.0f;
    m_viewport.maxDepth = 1.0f;

    // m_viewport.width = static_cast<f32>(m_swap_chain->getExtent().width);
    // m_viewport.height = static_cast<f32>(m_swap_chain->getExtent().height);
    // m_viewport.x = 0.0f;
    // m_viewport.y = 0.0f;
    // m_viewport.minDepth = 0.0f;
    // m_viewport.maxDepth = 1.0f;

    // Any pixels outside the scissor rectangles will be discarded by the rasterizer

    VkExtent2D extent;
    extent.width = m_render_context.width;
    extent.height = m_render_context.height;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = extent;

    VkPipelineViewportStateCreateInfo viewportStateCreateInfo{};
    viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportStateCreateInfo.viewportCount = 1;
    viewportStateCreateInfo.pViewports = &m_viewport;
    viewportStateCreateInfo.scissorCount = 1;
    viewportStateCreateInfo.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo{};
    rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
    rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
    rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizationStateCreateInfo.lineWidth = 1.0f;
    rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisamplingStateCreateInfo{};
    multisamplingStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisamplingStateCreateInfo.sampleShadingEnable = VK_FALSE;
    multisamplingStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    // multisamplingStateCreateInfo.minSampleShading = 1.0f; // Optional
    // multisamplingStateCreateInfo.pSampleMask = nullptr; // Optional
    // multisamplingStateCreateInfo.alphaToCoverageEnable = VK_FALSE; // Optional
    // multisamplingStateCreateInfo.alphaToOneEnable = VK_FALSE; // Optional

    VkPipelineDepthStencilStateCreateInfo depthStencilCreateInfo{};
    depthStencilCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilCreateInfo.depthTestEnable = VK_TRUE;
    depthStencilCreateInfo.depthWriteEnable = VK_TRUE;
    depthStencilCreateInfo.depthCompareOp = VK_COMPARE_OP_GREATER;
    depthStencilCreateInfo.depthBoundsTestEnable = VK_FALSE;
    depthStencilCreateInfo.stencilTestEnable = VK_FALSE;

    std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachmentStates(
        m_framebuffer->getColorAttachmentCount());
    for (auto &state : colorBlendAttachmentStates) {
        state.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        state.blendEnable = VK_FALSE;
    }
    // colorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
    // colorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
    // colorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD; // Optional
    // colorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
    // colorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
    // colorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD; // Optional

    VkPipelineColorBlendStateCreateInfo colorBlendingStateCreateInfo{};
    colorBlendingStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendingStateCreateInfo.logicOpEnable = VK_FALSE;
    colorBlendingStateCreateInfo.logicOp = VK_LOGIC_OP_COPY;
    colorBlendingStateCreateInfo.attachmentCount = static_cast<u32>(colorBlendAttachmentStates.size());
    colorBlendingStateCreateInfo.pAttachments = colorBlendAttachmentStates.data();
    colorBlendingStateCreateInfo.blendConstants[0] = 0.0f;
    colorBlendingStateCreateInfo.blendConstants[1] = 0.0f;
    colorBlendingStateCreateInfo.blendConstants[2] = 0.0f;
    colorBlendingStateCreateInfo.blendConstants[3] = 0.0f;

    std::array<VkDynamicState, 2> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_LINE_WIDTH};

    VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo{};
    dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicStateCreateInfo.dynamicStateCount = static_cast<u32>(dynamicStates.size());
    dynamicStateCreateInfo.pDynamicStates = dynamicStates.data();

    // create vk graphics pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = pipelineShaderStageCreateInfos.data();
    pipelineInfo.pVertexInputState = &vertexInputStateCreateInfo;
    pipelineInfo.pInputAssemblyState = &inputAssemblyStateCreateInfo;
    pipelineInfo.pViewportState = &viewportStateCreateInfo;
    pipelineInfo.pRasterizationState = &rasterizationStateCreateInfo;
    pipelineInfo.pDepthStencilState = &depthStencilCreateInfo;
    pipelineInfo.pMultisampleState = &multisamplingStateCreateInfo;
    pipelineInfo.pColorBlendState = &colorBlendingStateCreateInfo;
    pipelineInfo.layout = m_pipeline_layout;
    pipelineInfo.renderPass = getRenderPass();
    pipelineInfo.pDynamicState = &dynamicStateCreateInfo;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    CHECK_VK_RESULT(vkCreateGraphicsPipelines(m_device->Get(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline));
}

PipelineManager::PipelineManager(std::shared_ptr<Device> device) : m_device(device) {}

std::shared_ptr<Pipeline>
PipelineManager::CreateGraphicsPipeline(const GraphicsPipelineCreateInfo &create_info,
                                        const std::vector<AttachmentCreateInfo> &_attachment_create_info,
                                        const RenderContext _render_context) {
    std::string hashKey = GetPipelineKey(create_info);
    // pipeline key exist
    if (!m_pipeline_map[hashKey].pipeline) {
        auto &pipelineVal = m_pipeline_map[hashKey];
        pipelineVal.pipeline =
            std::make_shared<GraphicsPipeline>(m_device, create_info, _attachment_create_info, _render_context);
    } else {
        LOG_INFO("pipeline exist");
    }
    return m_pipeline_map[hashKey].pipeline;
}

std::shared_ptr<Pipeline> PipelineManager::CreateComputePipeline(const ComputePipelineCreateInfo &create_info) {
    std::string hashKey = GetPipelineKey(create_info);
    // pipeline key exist
    if (!m_pipeline_map[hashKey].pipeline) {
        auto &pipelineVal = m_pipeline_map[hashKey];
        pipelineVal.pipeline = std::make_shared<ComputePipeline>(m_device, create_info);
    } else {
        LOG_INFO("pipeline exist");
    }
    return m_pipeline_map[hashKey].pipeline;
}

void PipelineManager::createPresentPipeline(const GraphicsPipelineCreateInfo &create_info,
                                            const std::vector<AttachmentCreateInfo> &_attachment_create_info,
                                            RenderContext &_render_context, std::shared_ptr<SwapChain> swap_chain) {
    std::string hashKey = "present";
    // pipeline key exist
    if (!m_pipeline_map[hashKey].pipeline) {
        auto &pipelineVal = m_pipeline_map[hashKey];
        pipelineVal.pipeline = std::make_shared<GraphicsPipeline>(m_device, create_info, _attachment_create_info,
                                                                  _render_context, swap_chain);
    }
}

std::shared_ptr<Pipeline> PipelineManager::Get(const std::string &name) {
    std::string hashKey = name;
    if (m_pipeline_map[hashKey].pipeline) {
        return m_pipeline_map[hashKey].pipeline;
    } else {
        LOG_ERROR("no pipeline found");
        return nullptr;
    }
}

ComputePipeline::ComputePipeline(std::shared_ptr<Device> device, const ComputePipelineCreateInfo &create_info) noexcept
    : Pipeline(device) {
    m_group_count_x = create_info.group_count_x;
    m_group_count_y = create_info.group_count_y;
    m_group_count_z = create_info.group_count_z;
    m_type = PipelineType::COMPUTE;
    CreatePipelineLayout(create_info);
    CreatePipeline(create_info);
}

ComputePipeline::~ComputePipeline() noexcept {}

u32 ComputePipeline::GroupCountX() const noexcept { return m_group_count_x; }

u32 ComputePipeline::GroupCountY() const noexcept { return m_group_count_y; }

u32 ComputePipeline::GroupCountZ() const noexcept { return m_group_count_z; }

void ComputePipeline::CreatePipelineLayout(const ComputePipelineCreateInfo &create_info) noexcept {
    //auto &layouts = create_info.descriptor_layouts;
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    // prevent crash if no descriptors are used in shader
    if (create_info.descriptor_layouts) {
        pipelineLayoutInfo.setLayoutCount = static_cast<u32>(create_info.descriptor_layouts->layouts.size());
        pipelineLayoutInfo.pSetLayouts = create_info.descriptor_layouts->layouts.data();
    } else {
        pipelineLayoutInfo.setLayoutCount = 0;
        pipelineLayoutInfo.pSetLayouts = nullptr;
    }

    std::vector<VkPushConstantRange> push_constant_ranges;

    if (create_info.push_constants) {
        push_constant_ranges.resize(create_info.push_constants->ranges.size());
        m_push_constants = create_info.push_constants;

        for (u32 i = 0; i < m_push_constants->ranges.size(); i++) {
            push_constant_ranges[i].stageFlags =
                ToVkShaderStageFlags(m_push_constants->ranges[i].stages); // convert to vulkan shader stage flags
            push_constant_ranges[i].offset = m_push_constants->ranges[i].offset;
            push_constant_ranges[i].size = m_push_constants->ranges[i].size;
        }
        pipelineLayoutInfo.pushConstantRangeCount = push_constant_ranges.size();
        pipelineLayoutInfo.pPushConstantRanges = push_constant_ranges.data();
    }
    CHECK_VK_RESULT(vkCreatePipelineLayout(m_device->Get(), &pipelineLayoutInfo, nullptr, &m_pipeline_layout));
}

void ComputePipeline::CreatePipeline(const ComputePipelineCreateInfo &create_info) noexcept {

    VkPipelineShaderStageCreateInfo pipeline_shader_stage_create_info{};

    // vertex shader
    pipeline_shader_stage_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipeline_shader_stage_create_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipeline_shader_stage_create_info.module = create_info.cs->Get();
    pipeline_shader_stage_create_info.pName = "main";

    VkComputePipelineCreateInfo compute_pipeline_create_info{};
    compute_pipeline_create_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    compute_pipeline_create_info.flags = 0;
    compute_pipeline_create_info.layout = m_pipeline_layout;
    compute_pipeline_create_info.stage = pipeline_shader_stage_create_info;
    compute_pipeline_create_info.basePipelineHandle = nullptr;
    compute_pipeline_create_info.basePipelineIndex = 0;

    CHECK_VK_RESULT(
        vkCreateComputePipelines(m_device->Get(), nullptr, 1, &compute_pipeline_create_info, nullptr, &m_pipeline));
}

} // namespace Horizon