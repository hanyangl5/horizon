#pragma once
#include <memory>
#include <cstdint>
#include <string>
#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>
#include "Camera.h"
#include "Instance.h"
#include "Surface.h"
#include "Device.h"
#include "SwapChain.h"
#include "utils.h"

class Window;

class Renderer
{
public:
	Renderer(int width, int height, std::shared_ptr<Window> window);
	~Renderer();
	void Update();
	void Render();
	//void Destroy();
private:


	std::shared_ptr<Camera> mCamera;
	std::shared_ptr<Instance> mInstance;
	std::shared_ptr<Device> mDevice;
	std::shared_ptr<Surface> mSurface;
	std::shared_ptr<SwapChain> mSwapChain;


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
