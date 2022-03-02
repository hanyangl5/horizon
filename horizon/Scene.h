#pragma once

#include "utils.h"
#include "glm/glm.hpp"
#include "Camera.h"
#include "Device.h"
#include "Descriptors.h"
#include "Device.h"
#include "CommandBuffer.h"
#include "Model.h"
#include <vector>

class Scene {
public:
	Scene(Device* device, CommandBuffer* commandBuffer);
	~Scene();
	void destroy();
	void loadModel(const std::string& path);
	void prepare();
	void draw(Pipeline* pipeline);
	std::vector<DescriptorSet> getDescriptors();
private:
	Camera* mCamera;
	Device* mDevice;
	CommandBuffer* mCommandBuffer;

	// uniform buffers

	struct SceneUboStruct {
		glm::mat4 view;
		glm::mat4 projection;
	}sceneUboStruct;
	UniformBuffer* sceneUbo = nullptr;

	struct SceneUboStruct2 {
		float a = 5.0f;
		float b = 6.0f;
	}sceneUboStruct2;
	UniformBuffer* sceneUbo2 = nullptr;


	DescriptorSet* sceneDescritporSet = nullptr;
	// models
	std::vector<Model> mModels;
};