#pragma once

#include <runtime/function/rhi/Pipeline.h>

namespace Horizon
{
	namespace RHI
	{

		class VulkanPipeline : public Pipeline
		{
		public:
			VulkanPipeline(VkDevice device, const PipelineCreateInfo& pipeline_create_info) noexcept;
			~VulkanPipeline() noexcept;
			void Create() noexcept;
		public:
			VkDevice m_device;
			VkPipeline m_pipeline;
			const PipelineCreateInfo& m_create_info;
		};

	}
}