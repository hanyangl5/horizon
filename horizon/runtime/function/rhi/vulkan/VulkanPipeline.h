#pragma once

#include <runtime/function/rhi/Pipeline.h>
#include <runtime/function/rhi/ShaderProgram.h>
#include <runtime/function/rhi/vulkan/VulkanDescriptors.h>
namespace Horizon
{
	namespace RHI
	{

		class VulkanPipeline : public Pipeline
		{
		public:
			VulkanPipeline(VkDevice device, const PipelineCreateInfo& pipeline_create_info, VulkanDescriptor* descriptor) noexcept;
			~VulkanPipeline() noexcept;
			void Create() noexcept;
			virtual void SetShader(ShaderProgram* shader_moudle) noexcept override;
		private:
			void CreateGraphicsPipeline() noexcept;
			void CreateComputePipeline() noexcept;
			//void CreateRTPipeline() noexcept;
		public:
			VkDevice m_device;
			VkPipeline m_pipeline;
			VkPipelineLayout m_pipeline_layout;
			const PipelineCreateInfo& m_create_info;
			const VulkanDescriptor* m_descriptor;
		};

	}
}