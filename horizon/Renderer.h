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
#include "Pipeline.h"
#include "Framebuffers.h"
#include "Assets.h"

class Renderer
{
public:
	Renderer(u32 width, u32 height, Window* window);
	~Renderer();
	void Update();
	void Render();
	//void Destroy();
	void wait();
private:
	void drawFrame();
	void prepareAssests();

private:

	//Camera> mCamera;
	Window* mWindow = nullptr;
	Instance* mInstance = nullptr;
	Device* mDevice = nullptr;
	Surface* mSurface = nullptr;
	SwapChain* mSwapChain = nullptr;
	Pipeline* mPipeline = nullptr;
	Framebuffers* mFramebuffers = nullptr;
	CommandBuffer* mCommandBuffer = nullptr;
	static Assest mAssest;

	//sync primitives

	// semaphores
	VkSemaphore presenet_complete_semaphore;
	VkSemaphore render_complete_semaphore;
	std::vector<VkFence> fences;



};
