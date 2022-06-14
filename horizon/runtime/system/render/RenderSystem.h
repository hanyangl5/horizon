#pragma once

#include <vulkan/vulkan.h>

#include <runtime/function/rhi/RenderContext.h>
#include <runtime/core/window/Window.h>

#include <runtime/function/rhi/vulkan2/RHIVulkan.h>
#include <runtime/function/scene/camera/Camera.h>


namespace Horizon
{
	class RenderSystem
	{
	public:
		RenderSystem(u32 width, u32 height, std::shared_ptr<Window> window) noexcept;

		~RenderSystem() noexcept;
		
		void Tick() noexcept;

		std::shared_ptr<Camera> GetMainCamera() const noexcept;

	private:

		void Update() noexcept;

		void Render() noexcept;

		void InitializeRenderAPI(RenderBackend backend) noexcept;

		void RunRenderTest();
	private:
		RenderContext m_render_context;
		std::shared_ptr<Window> m_window = nullptr;

		std::shared_ptr<RHI::RHIInterface> m_render_api;
	};
}