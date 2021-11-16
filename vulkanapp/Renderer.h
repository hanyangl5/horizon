#include <memory>
#include <cstdint>
#include <string>
#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include "Camera.h"
#include "Instance.h"
class Renderer
{
public:
	Renderer(int width,int height, GLFWwindow* window);
	~Renderer();
	void Update();
	void Render();
	//void Destroy();
private:
	std::unique_ptr<Camera> mCamera;
	std::unique_ptr<Instance> mInstance;

	uint32_t glfwExtensionCount = 0;
	std::vector <std::string> glfwExtensions;
	//std::vector<VkExtensionProperties> glfwExtensions;


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
		uint32_t count;
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
