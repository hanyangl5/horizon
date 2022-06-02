#pragma once

#include <vulkan/vulkan.h>

#include <runtime/function/rhi/RenderContext.h>
#include <runtime/core/window/Window.h>
#include <runtime/function/rhi/vulkan/CommandBuffer.h>
#include <runtime/function/rhi/vulkan/Device.h>
#include <runtime/function/rhi/vulkan/SwapChain.h>
#include <runtime/function/rhi/vulkan/Descriptors.h>
#include <runtime/function/rhi/vulkan/Pipeline.h>
#include <runtime/function/rhi/vulkan/Framebuffer.h>
#include <runtime/function/rhi/vulkan/UniformBuffer.h>
#include <runtime/function/rhi/vulkan/RHIVulkan.h>
#include <runtime/function/scene/scene/Scene.h>
#include <runtime/system/render/render_passes/Atmosphere.h>
#include <runtime/system/render/render_passes/PostProcess.h>
#include <runtime/system/render/render_passes/Geometry.h>
#include <runtime/system/render/render_passes/LightPass.h>

namespace Horizon
{
	class RenderSystem
	{
	public:
		RenderSystem(u32 width, u32 height, std::shared_ptr<Window> window) noexcept;

		~RenderSystem() noexcept;
		
		void Tick() noexcept;

		void Wait() noexcept;

		std::shared_ptr<Camera> GetMainCamera() const noexcept;

	private:

		void Update() noexcept;

		void Render() noexcept;

		void DrawFrame() noexcept;

		void PrepareAssests() noexcept;

		// create pipeline layouts for each pass
		void CreatePipelines() noexcept;

		void CreatePresentPipeline() noexcept;

		void InitializeRenderAPI(RenderBackend backend) noexcept;

	private:
		RenderContext m_render_context;
		std::shared_ptr<Window> m_window = nullptr;
		std::shared_ptr<Device> m_device = nullptr;
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

		std::shared_ptr<Atmosphere> m_atmosphere_pass;
		std::shared_ptr<PostProcess> m_post_process_pass;
		std::shared_ptr<Geometry> m_geometry_pass;
		std::shared_ptr<LightPass> m_light_pass;

		std::shared_ptr<RHI::RHIInterface> m_render_api;
	};
}