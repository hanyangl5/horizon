#pragma once

#include <vulkan/vulkan.hpp>

#include <runtime/function/render/RenderContext.h>
#include <runtime/function/window/Window.h>
#include <runtime/function/render/rhi/CommandBuffer.h>
#include <runtime/function/render/rhi/Instance.h>
#include <runtime/function/render/rhi/Surface.h>
#include <runtime/function/render/rhi/Device.h>
#include <runtime/function/render/rhi/SwapChain.h>
#include <runtime/function/render/rhi/Descriptors.h>
#include <runtime/function/render/rhi/Pipeline.h>
#include <runtime/function/render/rhi/Framebuffer.h>
#include <runtime/function/render/rhi/UniformBuffer.h>

#include <runtime/scene/scene/Scene.h>

namespace Horizon
{
	class Renderer
	{
	public:
		Renderer(u32 width, u32 height, std::shared_ptr<Window> window);

		~Renderer();

		void Init();

		void Update();

		void Render();

		void Wait();

		std::shared_ptr<Camera> GetMainCamera() const;

	private:
		void DrawFrame();

		void PrepareAssests();

		// create pipeline layouts for each pass
		void CreatePipelines();

		RenderContext m_render_context;
		std::shared_ptr<Window> m_window = nullptr;
		std::shared_ptr<Instance> m_instance = nullptr;
		std::shared_ptr<Device> m_device = nullptr;
		std::shared_ptr<Surface> m_surface = nullptr;
		std::shared_ptr<SwapChain> m_swap_chain = nullptr;
		std::shared_ptr<PipelineManager> m_pipeline_manager = nullptr;
		std::shared_ptr<CommandBuffer> m_command_buffer = nullptr;
		std::shared_ptr<Scene> m_scene = nullptr;
		std::shared_ptr<FullscreenTriangle> m_fullscreen_triangle = nullptr;
		// sync primitives

		// semaphores
		VkSemaphore m_presenet_complete_semaphore;
		VkSemaphore render_complete_semaphore;
		std::vector<VkFence> m_fences;

		// pipeline objects
		std::shared_ptr<DescriptorSet> m_present_descriptorSet;
		std::shared_ptr<DescriptorSet> m_scatter_descriptorSet;
		std::shared_ptr<DescriptorSet> m_pp_descriptorSet;

		
		std::shared_ptr<UniformBuffer> m_scattering_ub;
		struct ScatteringUb {
			Math::mat4 inv_view_projection_matrix;
			Math::vec2 resolution;
		}m_scattering_ubdata;
	};
}