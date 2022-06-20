#include "VulkanPipeline.h"


namespace Horizon
{
	namespace RHI
	{

		VulkanPipeline::VulkanPipeline(VkDevice device, const PipelineCreateInfo& pipeline_create_info) noexcept :m_device(device), m_create_info(pipeline_create_info)
		{

		}

		VulkanPipeline::~VulkanPipeline() noexcept
		{
		}

		void VulkanPipeline::Create() noexcept
		{

			//VkPipelineLayoutCreateInfo pipeline_layout_create_info{};


			//switch (pipeline_create_info.type)
			//{
			//case PipelineType::COMPUTE:
			//	VkComputePipelineCreateInfo compute_pipeline_create_info{};

			//	compute_pipeline_create_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
			//	compute_pipeline_create_info.flags = 0;
			//	compute_pipeline_create_info.layout = m_pipeline_layout;
			//	compute_pipeline_create_info.stage = pipeline_shader_stage_create_info;
			//	compute_pipeline_create_info.basePipelineHandle = nullptr;
			//	compute_pipeline_create_info.basePipelineIndex = 0;
			//	vkCreateComputePipelines(m_device, nullptr, 1, &compute_pipeline_create_info, nullptr, &m_pipeline);

			//case PipelineType::GRAPHICS:
			//	VkGraphicsPipelineCreateInfo graphics_pipeline_create_info{};

			//	vkCreateGraphicsPipelines(m_device, nullptr, 1, &graphics_pipeline_create_info, nullptr, &m_pipeline);
			//case PipelineType::RAY_TRACING:
			//	//VkRayTracingPipelineCreateInfoKHR rt_pipeline_create_info{};
			//	//vkCreateRayTracingPipelinesKHR();
			//default:
			//	break;
			//}

		}
	}
}