#include "Pipeline.h"
#include "Instance.h"
#include "Device.h"
#include "SwapChain.h"
#include "Surface.h"
#include "ShaderModule.h"
#include <array>
Pipeline::Pipeline(std::shared_ptr<Device> device,
	std::shared_ptr<SwapChain> swapchain) : mDevice(device), mSwapChain(swapchain)
{
	createPipelineLayout();
	createRenderPass();
	createPipeline();
}

Pipeline::~Pipeline()
{
	vkDestroyPipelineLayout(mDevice->get(), mPipelineLayout, nullptr);
}

VkPipeline Pipeline::get() const
{
	return mGraphicsPipeline;
}

VkRenderPass Pipeline::getRenderPass() const
{
	return mRenderPass;
}

VkViewport Pipeline::getViewport() const
{
	return mViewport;
}

void Pipeline::createPipelineLayout()
{
	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 0;
	pipelineLayoutInfo.pushConstantRangeCount = 0;

	printVkError(vkCreatePipelineLayout(mDevice->get(), &pipelineLayoutInfo, nullptr, &mPipelineLayout), "create pipeline layout");

}

void Pipeline::createRenderPass()
{

	// renderpass
	VkAttachmentDescription colorAttachment{};
	colorAttachment.format = mSwapChain->getImageFormat();
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference colorAttachmentRef{};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;

	VkRenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = 1;
	renderPassInfo.pAttachments = &colorAttachment;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;

	printVkError(vkCreateRenderPass(mDevice->get(), &renderPassInfo, nullptr, &mRenderPass), "create render pass", logLevel::debug);

}

void Pipeline::createPipeline()
{

	Shader vs(mDevice->get(), "C:/Users/hylu/OneDrive/mycode/vulkan/shaders/basicvert.spv");
	Shader ps(mDevice->get(), "C:/Users/hylu/OneDrive/mycode/vulkan/shaders/basicfrag.spv");

	std::array<VkPipelineShaderStageCreateInfo, 2> pipelineShaderStageCreateInfos{};

	// vertex shader
	pipelineShaderStageCreateInfos[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	pipelineShaderStageCreateInfos[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	pipelineShaderStageCreateInfos[0].module = vs.get();
	pipelineShaderStageCreateInfos[0].pName = "main";

	//pixel shader
	pipelineShaderStageCreateInfos[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	pipelineShaderStageCreateInfos[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	pipelineShaderStageCreateInfos[1].module = ps.get();
	pipelineShaderStageCreateInfos[1].pName = "main";

	VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo{};
	vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputStateCreateInfo.vertexBindingDescriptionCount = 0;
	vertexInputStateCreateInfo.pVertexBindingDescriptions = nullptr; // Optional
	vertexInputStateCreateInfo.vertexAttributeDescriptionCount = 0;
	vertexInputStateCreateInfo.pVertexAttributeDescriptions = nullptr; // Optional

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo{};
	inputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;

	// A viewport basically describes the region of the framebuffer that the output will be rendered to
	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<f32>(mSwapChain->getExtent().width);
	viewport.height = static_cast<f32>(mSwapChain->getExtent().height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	mViewport = viewport;
	//Any pixels outside the scissor rectangles will be discarded by the rasterizer
	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = mSwapChain->getExtent();

	VkPipelineViewportStateCreateInfo viewportStateCreateInfo{};
	viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateCreateInfo.viewportCount = 1;
	viewportStateCreateInfo.pViewports = &viewport;
	viewportStateCreateInfo.scissorCount = 1;
	viewportStateCreateInfo.pScissors = &scissor;

	VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo{};
	rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
	rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
	rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizationStateCreateInfo.lineWidth = 1.0f;
	rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;

	VkPipelineMultisampleStateCreateInfo multisamplingStateCreateInfo{};
	multisamplingStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisamplingStateCreateInfo.sampleShadingEnable = VK_FALSE;
	multisamplingStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	//multisamplingStateCreateInfo.minSampleShading = 1.0f; // Optional
	//multisamplingStateCreateInfo.pSampleMask = nullptr; // Optional
	//multisamplingStateCreateInfo.alphaToCoverageEnable = VK_FALSE; // Optional
	//multisamplingStateCreateInfo.alphaToOneEnable = VK_FALSE; // Optional


	VkPipelineColorBlendAttachmentState colorBlendAttachmentState{};
	colorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachmentState.blendEnable = VK_FALSE;
	//colorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
	//colorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
	//colorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD; // Optional
	//colorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
	//colorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
	//colorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD; // Optional

	VkPipelineColorBlendStateCreateInfo colorBlendingStateCreateInfo{};
	colorBlendingStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendingStateCreateInfo.logicOpEnable = VK_FALSE;
	colorBlendingStateCreateInfo.logicOp = VK_LOGIC_OP_COPY;
	colorBlendingStateCreateInfo.attachmentCount = 1;
	colorBlendingStateCreateInfo.pAttachments = &colorBlendAttachmentState;
	colorBlendingStateCreateInfo.blendConstants[0] = 0.0f;
	colorBlendingStateCreateInfo.blendConstants[1] = 0.0f;
	colorBlendingStateCreateInfo.blendConstants[2] = 0.0f;
	colorBlendingStateCreateInfo.blendConstants[3] = 0.0f;

	std::array<VkDynamicState, 2> dynamicStates = {
	VK_DYNAMIC_STATE_VIEWPORT,
	VK_DYNAMIC_STATE_LINE_WIDTH
	};

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
	pipelineInfo.pMultisampleState = &multisamplingStateCreateInfo;
	pipelineInfo.pColorBlendState = &colorBlendingStateCreateInfo;
	pipelineInfo.layout = mPipelineLayout;
	pipelineInfo.renderPass = mRenderPass;
	pipelineInfo.pDynamicState = &dynamicStateCreateInfo;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

	printVkError(vkCreateGraphicsPipelines(mDevice->get(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &mGraphicsPipeline), "create graphics pipeline");
}
