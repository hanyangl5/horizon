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
#include "Light.h"

#define MAX_LIGHT_COUNT 1024

class Scene {
public:
	Scene(Device* device, CommandBuffer* commandBuffer);
	~Scene();
	void destroy();
	void loadModel(const std::string& path);
	void addDirectLight(glm::vec3 color, f32 intensity, glm::vec3 direction);
	void addPointLight(glm::vec3 color, f32 intensity, glm::vec3 position, f32 radius);
	void addSpotLight(glm::vec3 color, f32 intensity, glm::vec3 direction, glm::vec3 position, f32 innerRadius, f32 outerRadius);
	void prepare();
	void draw(Pipeline* pipeline);
	std::vector<DescriptorSet> getDescriptors();
private:
	Camera* mCamera;
	Device* mDevice;
	CommandBuffer* mCommandBuffer;
	DescriptorSet* sceneDescritporSet = nullptr;

	// uniform buffers

	// 0
	struct SceneUbStruct {
		glm::mat4 view;
		glm::mat4 projection;
	}sceneUbStruct;
	UniformBuffer* sceneUb = nullptr;
	// 1
	struct LightCountUbStruct {
		u32 lightCount = 0;
	}lightCountUbStruct;
	UniformBuffer* lightCountUb;
	// 2
	struct LightsUbStruct {
		LightParams lights[MAX_LIGHT_COUNT];
	}lightUbStruct;
	UniformBuffer* lightUb;
	// 3
	struct CamaeraUbStruct {
		glm::vec3 cameraPos;
	}camaeraUbStruct;
	UniformBuffer* cameraUb;

	// models
	std::vector<Model> mModels;
};