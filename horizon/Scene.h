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
		Scene(Device* device, CommandBuffer* commandBuffer, u32 w, u32 h);
		~Scene();
		void destroy();
		void loadModel(const std::string& path);
		void addDirectLight(Math::vec3 color, f32 intensity, Math::vec3 direction);
		void addPointLight(Math::vec3 color, f32 intensity, Math::vec3 position, f32 radius);
		void addSpotLight(Math::vec3 color, f32 intensity, Math::vec3 direction, Math::vec3 position, f32 radius, f32 innerConeAngle, f32 outerConeAngle);

		void prepare();
		void draw(Pipeline* pipeline);
		std::vector<DescriptorSet> getDescriptors();
		DescriptorSetLayouts getDescriptorLayouts();
		Camera* getMainCamera() const;
	private:
		Camera* mCamera = nullptr;
		Device* mDevice;
		CommandBuffer* mCommandBuffer;
		DescriptorSet* sceneDescritporSet = nullptr;

		u32 mWidth, mHeight;


		// uniform buffers

		// 0
		struct SceneUbStruct {
			Math::mat4 view;
			Math::mat4 projection;
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
			Math::vec3 cameraPos;
		}camaeraUbStruct;
		UniformBuffer* cameraUb;


		// models
		std::vector<Model> mModels;

		// 
	};
}