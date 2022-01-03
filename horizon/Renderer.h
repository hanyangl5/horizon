#pragma once
#include <memory>
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


class Renderer
{
public:
	Renderer(u32 width, u32 height, std::shared_ptr<Window> window);
	~Renderer();
	void Update();
	void Render();
	//void Destroy();
	void wait();
private:
	void drawFrame();
private:

	//std::shared_ptr<Camera> mCamera;
	std::shared_ptr<Window> mWindow;
	std::shared_ptr<Instance> mInstance;
	std::shared_ptr<Device> mDevice;
	std::shared_ptr<Surface> mSurface;
	std::shared_ptr<SwapChain> mSwapChain;
	std::shared_ptr<Pipeline> mPipeline;
	std::shared_ptr<Framebuffers> mFramebuffers;
	std::shared_ptr<CommandBuffer> mCommandBuffer;
	
	// resource
	struct Vertex {
		glm::vec3 position;
		glm::vec3 color;
	};
	// vertex buffer
	struct {
		VkDeviceMemory memory;
		VkBuffer buffer;
	}vertices;
	// index buffer
	struct {
		VkDeviceMemory memory;
		VkBuffer buffer;
		u32 count;
	}indices;

	// uniform buffer block object

	struct {
		VkDeviceMemory memory;
		VkBuffer buffer;
		VkDescriptorBufferInfo descriptor;
	}uniformBufferVS;

	struct {
		glm::mat4 projmat;
		glm::mat4 modelmat;
		glm::mat4 viewmat;
	}uboVS;
	VkPipelineLayout pipeline_layout; //pipeline layout is used to access descriptroset
	VkPipeline pipeline;
	VkDescriptorSetLayout descriptor_set_layout;
	VkDescriptorSet descriptor_set;


	//sync primitives

	// semaphores
	VkSemaphore presenet_complete_semaphore;
	VkSemaphore render_complete_semaphore;
	std::vector<VkFence> fences;



};
