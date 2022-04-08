#pragma once

#include <vulkan/vulkan.hpp>

#include "utils.h"
#include "Window.h"
#include "CommandBuffer.h"
#include "Instance.h"
#include "Surface.h"
#include "Device.h"
#include "SwapChain.h"
#include "Descriptors.h"
#include "Pipeline.h"
#include "Framebuffer.h"
#include "Scene.h"
#include "UniformBuffer.h"

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

		void wait();

		std::shared_ptr<Camera> getMainCamera() const;

	private:
		void drawFrame();

		void prepareAssests();

		// create pipeline layouts for each pass
		void createPipelines();

		RenderContext mRenderContext;
		std::shared_ptr<Window> mWindow = nullptr;
		std::shared_ptr<Instance> mInstance = nullptr;
		std::shared_ptr<Device> mDevice = nullptr;
		std::shared_ptr<Surface> mSurface = nullptr;
		std::shared_ptr<SwapChain> mSwapChain = nullptr;
		std::shared_ptr<PipelineManager> mPipelineMgr = nullptr;
		std::shared_ptr<CommandBuffer> mCommandBuffer = nullptr;
		std::shared_ptr<Scene> mScene = nullptr;
		std::shared_ptr<FullscreenTriangle> mFullscreenTriangle = nullptr;
		// sync primitives

		// semaphores
		VkSemaphore presenet_complete_semaphore;
		VkSemaphore render_complete_semaphore;
		std::vector<VkFence> fences;

		// pipeline objects
		std::shared_ptr<DescriptorSet> presentDescriptorSet;
		std::shared_ptr<DescriptorSet> scatterDescriptorSet;
		std::shared_ptr<DescriptorSet> ppDescriptorSet;
	};
}