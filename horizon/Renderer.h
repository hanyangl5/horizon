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
#include "Framebuffers.h"
#include "Scene.h"
#include "UniformBuffer.h"

namespace Horizon {

	class Renderer
	{
	public:
		Renderer(u32 width, u32 height, Window* window);

		~Renderer();

		void Init();

		void Update();

		void Render();

		void Destroy();

		void wait();

		Camera* getMainCamera() const;

	private:

		void drawFrame();

		void prepareAssests();

		void releaseAssets();
		// create pipeline layouts for each pass
		void createPipelines();

		u32 mWidth, mHeight;
		Window* mWindow = nullptr;
		Instance* mInstance = nullptr;
		Device* mDevice = nullptr;
		Surface* mSurface = nullptr;
		SwapChain* mSwapChain = nullptr;
		//RenderPass* mRenderPass = nullptr;
		//Pipeline* mPipeline = nullptr;
		PipelinaManager mPipelineMgr;
		Framebuffers* mFramebuffers = nullptr;
		CommandBuffer* mCommandBuffer = nullptr;
		Scene* mScene;

		//sync primitives

		// semaphores
		VkSemaphore presenet_complete_semaphore;
		VkSemaphore render_complete_semaphore;
		std::vector<VkFence> fences;

	};
}