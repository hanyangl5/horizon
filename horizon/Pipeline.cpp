#include "Pipeline.h"

#include <array>

#include "Instance.h"
#include "Device.h"
#include "SwapChain.h"
#include "Surface.h"
#include "ShaderModule.h"
#include "Vertex.h"

namespace Horizon {

	Pipeline::Pipeline(std::shared_ptr<Device> device, const PipelineCreateInfo& createInfo, const std::vector<AttachmentCreateInfo>& attachmentsCreateInfo, RenderContext& renderContext, std::shared_ptr<SwapChain> swapChain) :mRenderContext(renderContext), mDevice(device)
	{
		if (swapChain) {
			mFramebuffer = std::make_shared<Framebuffer>(mDevice, attachmentsCreateInfo, mRenderContext, swapChain);
		}
		else {
			mFramebuffer = std::make_shared<Framebuffer>(mDevice, attachmentsCreateInfo, mRenderContext);
		}
		createPipelineLayout(createInfo);
		createPipeline(createInfo);
		mClearValues = mFramebuffer->getClearValues();
	}

	Pipeline::~Pipeline() {
		vkDestroyPipeline(mDevice->get(), mGraphicsPipeline, nullptr);
		vkDestroyPipelineLayout(mDevice->get(), mPipelineLayout, nullptr);
	}


	VkPipeline Pipeline::get() const
	{
		return mGraphicsPipeline;
	}

	VkPipelineLayout Pipeline::getLayout() const
	{
		return mPipelineLayout;
	}


	VkViewport Pipeline::getViewport() const
	{
		return mViewport;
	}

	VkRenderPass Pipeline::getRenderPass() const
	{
		return mFramebuffer->getRenderPass();
	}

	VkFramebuffer Pipeline::getFrameBuffer() const
	{
		return mFramebuffer->get();
	}
	VkFramebuffer Pipeline::getFrameBuffer(u32 index) const
	{
		return mFramebuffer->get(index);
	}
	std::shared_ptr<AttachmentDescriptor> Pipeline::getFramebufferDescriptorImageInfo(u32 attachmentIndex)
	{
		return mFramebuffer->getDescriptorImageInfo(attachmentIndex);
	}


	std::vector<VkImage> Pipeline::getPresentImages()
	{
		return std::vector<VkImage>();
	}

	std::vector<VkClearValue> Pipeline::getClearValues()
	{
		return mClearValues;
	}

	bool Pipeline::hasPushConstants()
	{
		return mPushConstants != nullptr;
	}

	bool Pipeline::hasPipelineDescriptorSet()
	{
		return mPipelineDescriptorSet != nullptr;
	}

	std::shared_ptr<DescriptorSet> Pipeline::getPipelineDescriptorSet()
	{
		return mPipelineDescriptorSet;
	}


	void Pipeline::createPipelineLayout(const PipelineCreateInfo& createInfo)
	{
		auto& layouts = createInfo.descriptorLayouts;
		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.pushConstantRangeCount = 0;
		// prevent crash if no descriptors are used in shader
		if (createInfo.descriptorLayouts || createInfo.descriptorLayouts->layouts.empty()) {
			pipelineLayoutInfo.setLayoutCount = static_cast<u32>(createInfo.descriptorLayouts->layouts.size());
			pipelineLayoutInfo.pSetLayouts = createInfo.descriptorLayouts->layouts.data();
		}
		else {
			pipelineLayoutInfo.setLayoutCount = 0;
			pipelineLayoutInfo.pSetLayouts = nullptr;
		}
		if (createInfo.pushConstants) {

			mPushConstants = createInfo.pushConstants;
			pipelineLayoutInfo.pushConstantRangeCount = mPushConstants->pushConstantRanges.size();
			pipelineLayoutInfo.pPushConstantRanges = mPushConstants->pushConstantRanges.data();
		}
		printVkError(vkCreatePipelineLayout(mDevice->get(), &pipelineLayoutInfo, nullptr, &mPipelineLayout), "create pipeline layout");

		mPipelineDescriptorSet = createInfo.pipelineDescriptorSet;
	}

	void Pipeline::createPipeline(const PipelineCreateInfo& createInfo)
	{

		std::array<VkPipelineShaderStageCreateInfo, 2> pipelineShaderStageCreateInfos{};

		// vertex shader
		pipelineShaderStageCreateInfos[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		pipelineShaderStageCreateInfos[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
		pipelineShaderStageCreateInfos[0].module = createInfo.vs->get();
		pipelineShaderStageCreateInfos[0].pName = "main";

		//pixel shader
		pipelineShaderStageCreateInfos[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		pipelineShaderStageCreateInfos[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		pipelineShaderStageCreateInfos[1].module = createInfo.ps->get();
		pipelineShaderStageCreateInfos[1].pName = "main";

		auto& bindingDescription = Vertex::getBindingDescription();
		auto& attributeDescriptions = Vertex::getAttributeDescriptions();

		VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo{};
		vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputStateCreateInfo.vertexBindingDescriptionCount = 1;
		vertexInputStateCreateInfo.pVertexBindingDescriptions = &bindingDescription; // Optional
		vertexInputStateCreateInfo.vertexAttributeDescriptionCount = static_cast<u32>(attributeDescriptions.size());;
		vertexInputStateCreateInfo.pVertexAttributeDescriptions = attributeDescriptions.data(); // Optional

		VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo{};
		inputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;

		// A viewport basically describes the region of the framebuffer that the output will be rendered to
		mViewport.width = static_cast<f32>(mRenderContext.width);
		mViewport.height = -static_cast<f32>(mRenderContext.height);
		mViewport.x = 0.0f;
		mViewport.y = -mViewport.height;
		mViewport.minDepth = 0.0f;
		mViewport.maxDepth = 1.0f;

		//mViewport.width = static_cast<f32>(mSwapChain->getExtent().width);
		//mViewport.height = static_cast<f32>(mSwapChain->getExtent().height);
		//mViewport.x = 0.0f;
		//mViewport.y = 0.0f;
		//mViewport.minDepth = 0.0f;
		//mViewport.maxDepth = 1.0f;

		//Any pixels outside the scissor rectangles will be discarded by the rasterizer

		VkExtent2D extent;
		extent.width = mRenderContext.width;
		extent.height = mRenderContext.height;

		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = extent;

		VkPipelineViewportStateCreateInfo viewportStateCreateInfo{};
		viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportStateCreateInfo.viewportCount = 1;
		viewportStateCreateInfo.pViewports = &mViewport;
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
		//multisamplingStateCreateInfo.minSampleShading = 1.0f; // Optional
		//multisamplingStateCreateInfo.pSampleMask = nullptr; // Optional
		//multisamplingStateCreateInfo.alphaToCoverageEnable = VK_FALSE; // Optional
		//multisamplingStateCreateInfo.alphaToOneEnable = VK_FALSE; // Optional

		VkPipelineDepthStencilStateCreateInfo depthStencilCreateInfo{};
		depthStencilCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencilCreateInfo.depthTestEnable = VK_TRUE;
		depthStencilCreateInfo.depthWriteEnable = VK_TRUE;
		depthStencilCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS;
		depthStencilCreateInfo.depthBoundsTestEnable = VK_FALSE;
		depthStencilCreateInfo.stencilTestEnable = VK_FALSE;

		std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachmentStates(mFramebuffer->getColorAttachmentCount());
		for (auto& state : colorBlendAttachmentStates) {
			state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
			state.blendEnable = VK_FALSE;
		}
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
		colorBlendingStateCreateInfo.attachmentCount = static_cast<u32>(colorBlendAttachmentStates.size());
		colorBlendingStateCreateInfo.pAttachments = colorBlendAttachmentStates.data();
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
		pipelineInfo.pDepthStencilState = &depthStencilCreateInfo;
		pipelineInfo.pMultisampleState = &multisamplingStateCreateInfo;
		pipelineInfo.pColorBlendState = &colorBlendingStateCreateInfo;
		pipelineInfo.layout = mPipelineLayout;
		pipelineInfo.renderPass = getRenderPass();
		pipelineInfo.pDynamicState = &dynamicStateCreateInfo;
		pipelineInfo.subpass = 0;
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

		printVkError(vkCreateGraphicsPipelines(mDevice->get(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &mGraphicsPipeline), "create graphics pipeline failed");
	}



	PipelineManager::PipelineManager(std::shared_ptr<Device> device) :mDevice(device)
	{
	}

	void PipelineManager::createPipeline(const PipelineCreateInfo& createInfo, const std::string& name, const std::vector<AttachmentCreateInfo>& attachmentsCreateInfo, RenderContext& renderContext)
	{
		u32 hashKey = pipelineHash(name);
		// pipeline key exist
		if (!mPipelineMap[hashKey].pipeline) {

			auto& pipelineVal = mPipelineMap[hashKey];
			pipelineVal.pipeline = std::make_shared<Pipeline>(mDevice, createInfo, attachmentsCreateInfo, renderContext);
		}
	}

	void PipelineManager::createPresentPipeline(const PipelineCreateInfo& createInfo, const std::vector<AttachmentCreateInfo>& attachmentsCreateInfo, RenderContext& renderContext, std::shared_ptr<SwapChain> swapChain)
	{
		u32 hashKey = pipelineHash("present");
		// pipeline key exist
		if (!mPipelineMap[hashKey].pipeline) {

			auto& pipelineVal = mPipelineMap[hashKey];
			pipelineVal.pipeline = std::make_shared<Pipeline>(mDevice, createInfo, attachmentsCreateInfo, renderContext, swapChain);
		}
	}

	std::shared_ptr<Pipeline> PipelineManager::get(const std::string& name)
	{
		u32 hashKey = pipelineHash(name);
		if (mPipelineMap[hashKey].pipeline) {
			return mPipelineMap[hashKey].pipeline;
		}
		else {
			spdlog::error("no pipeline found");
			return nullptr;
		}
	}



}