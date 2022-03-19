#pragma once

#include <vector>

#include "utils.h"
#include "Camera.h"
#include "Device.h"
#include "Descriptors.h"
#include "CommandBuffer.h"
#include "Model.h"
#include "Light.h"

namespace Horizon {

#define MAX_LIGHT_COUNT 1024

	class Scene {
	public:
		Scene(Device* device, CommandBuffer* commandBuffer);
		~Scene();
		void destroy();
		void loadModel(const std::string& path);
		void addDirectLight(vec3 color, f32 intensity, vec3 direction);
		void addPointLight(vec3 color, f32 intensity, vec3 position, f32 radius);
		void addSpotLight(vec3 color, f32 intensity, vec3 direction, vec3 position, f32 innerRadius, f32 outerRadius);
		void prepare();
		void draw(Pipeline* pipeline);
		std::vector<DescriptorSet> getDescriptors();
	private:
		Camera* mCamera;
		Device* mDevice;
		CommandBuffer* mCommandBuffer;
		DescriptorSet* sceneDescritporSet = nullptr;

		// models
		std::vector<Model> mModels;

		// uniform buffers

		// 0
		struct SceneUbStruct {
			mat4 view;
			mat4 projection;
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
			vec3 cameraPos;
		}camaeraUbStruct;
		UniformBuffer* cameraUb;
	};
}