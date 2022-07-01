#pragma once

#include <vector>
#include <unordered_map>

#include <runtime/function/rhi/RHIUtils.h>
#include <runtime/function/rhi/ShaderProgram.h>

namespace Horizon
{
	namespace RHI
	{
		// struct viewport create info
		struct ViewportCreateInfo
		{
			u32 x;
			u32 y;
			u32 width;
			u32 height;
			f32 min_depth;
			f32 max_depth;
		};
		// vertex input state vk
		struct VertexInputState
		{
			std::vector<VkVertexInputBindingDescription> binding_descriptions;
			std::vector<VkVertexInputAttributeDescription> attribute_descriptions;
		};

		struct VertexInputStateDx12
		{
			std::vector<D3D12_INPUT_ELEMENT_DESC> input_element_descs;
			D3D12_INPUT_LAYOUT_DESC input_layout_desc;
		};
		
		// rasterization state dx12
		struct RasterizationStateDx12
		{
			D3D12_RASTERIZER_DESC rasterizer_desc;
		};

		struct RasterizationState
		{
			VkPolygonMode polygon_mode;
			VkCullModeFlags cull_mode;
			VkFrontFace front_face;
			VkBool32 depth_clamp;
			VkBool32 rasterizer_discard_enable;
			f32 depth_bias_constant_factor;
			f32 depth_bias_clamp;
			f32 depth_bias_slope_factor;
			f32 line_width;
		};

		class Pipeline
		{
		public:
			Pipeline() noexcept;
			~Pipeline() noexcept;

			PipelineType GetType() const noexcept;

			virtual void SetShader(ShaderProgram* shader_moudle) noexcept = 0;
			//// vertex input state
			//virtual void SetVertexInputState(const VertexInputStateCreateInfo& vertex_input_state_create_info) noexcept = 0;
			//// input assembly state
			//virtual void SetInputAssemblyState(const InputAssemblyStateCreateInfo& input_assembly_state_create_info) noexcept = 0;
			//// viewport state
			//virtual void SetViewportState(const ViewportStateCreateInfo& viewport_state_create_info) noexcept = 0;
			//// rasterization state
			//virtual void SetRasterizationState(const RasterizationStateCreateInfo& rasterization_state_create_info) noexcept = 0;
			//// multisample state
			//virtual void SetMultisampleState(const MultisampleStateCreateInfo& multisample_state_create_info) noexcept = 0;
			//// depth stencil state
			//virtual void SetDepthStencilState(const DepthStencilStateCreateInfo& depth_stencil_state_create_info) noexcept = 0;
			//// color blend state
			//virtual void SetColorBlendState(const ColorBlendStateCreateInfo& color_blend_state_create_info) noexcept = 0;
			//// dynamic state
			//virtual void SetDynamicState(const DynamicStateCreateInfo& dynamic_state_create_info) noexcept = 0;
			//// pipeline layout
			//virtual void SetPipelineLayout(const PipelineLayoutCreateInfo& pipeline_layout_create_info) noexcept = 0;
			//// render pass
			//virtual void SetRenderPass(const RenderPassCreateInfo& render_pass_create_info) noexcept = 0;


		protected:
			PipelineType m_type;
			//std::pair<ShaderType, ShaderMoudle> shader_map;
			//std::vector<ShaderMoudle> shaders;
			std::unordered_map<ShaderType, ShaderProgram*> shader_map;
		};

	}
}
