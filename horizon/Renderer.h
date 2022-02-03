#pragma once
#include <string>
#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>
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
#include "Assets.h"
#include "UniformBuffer.h"
class Renderer
{
public:
	Renderer(u32 width, u32 height, Window* window);
	~Renderer();
	void Init();
	void Update();
	void Render();
	//void Destroy();
	void wait();
private:
	void drawFrame();
	void prepareAssests();
private:
	void releaseAssets();
	//Camera> mCamera;
	Window* mWindow = nullptr;
	Instance* mInstance = nullptr;
	Device* mDevice = nullptr;
	Surface* mSurface = nullptr;
	SwapChain* mSwapChain = nullptr;
	Descriptors* mDescriptors = nullptr;
	Pipeline* mPipeline = nullptr;
	Framebuffers* mFramebuffers = nullptr;
	CommandBuffer* mCommandBuffer = nullptr;
	static Assest mAssest;

	// ubo
	struct colorStruct {
		float a = 1.0;
		float b = 0.0;
		float c = 0.0;
	};
	colorStruct colorubo;
	UniformBuffer* testUBO = nullptr;
	
	//sync primitives

	// semaphores
	VkSemaphore presenet_complete_semaphore;
	VkSemaphore render_complete_semaphore;
	std::vector<VkFence> fences;



};
