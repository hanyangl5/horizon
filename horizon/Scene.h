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
		Scene(RenderContext& renderContext, std::shared_ptr<Device> device, std::shared_ptr<CommandBuffer> commandBuffer);
		~Scene();

		void loadModel(const std::string& path);
		void addDirectLight(Math::vec3 color, f32 intensity, Math::vec3 direction);
		void addPointLight(Math::vec3 color, f32 intensity, Math::vec3 position, f32 radius);
		void addSpotLight(Math::vec3 color, f32 intensity, Math::vec3 direction, Math::vec3 position, f32 radius, f32 innerConeAngle, f32 outerConeAngle);

		void prepare();
		void draw(VkCommandBuffer commandBuffer, std::shared_ptr<Pipeline> pipeline);
		std::shared_ptr<DescriptorSetLayouts> getDescriptorLayouts();
		std::shared_ptr<Camera> getMainCamera() const;
	private:
		RenderContext& mRenderContext;
		std::shared_ptr<Camera> mCamera = nullptr;
		std::shared_ptr<Device> mDevice;
		std::shared_ptr<CommandBuffer> mCommandBuffer;
		std::shared_ptr<DescriptorSet> sceneDescritporSet = nullptr;

		// uniform buffers

		// 0
		struct SceneUbStruct {
			Math::mat4 view;
			Math::mat4 projection;
		}sceneUbStruct;
		std::shared_ptr<UniformBuffer> sceneUb = nullptr;
		// 1
		struct LightCountUbStruct {
			u32 lightCount = 0;
		}lightCountUbStruct;
		std::shared_ptr<UniformBuffer> lightCountUb;
		// 2
		struct LightsUbStruct {
			LightParams lights[MAX_LIGHT_COUNT];
		}lightUbStruct;
		std::shared_ptr<UniformBuffer> lightUb;
		// 3
		struct CamaeraUbStruct {
			Math::vec3 cameraPos;
		}camaeraUbStruct;
		std::shared_ptr<UniformBuffer> cameraUb;


		// models
		std::vector<std::shared_ptr<Model>> mModels;

	};

	class FullscreenTriangle {
	public:
		FullscreenTriangle(std::shared_ptr<Device> device, std::shared_ptr<CommandBuffer> commandBuffer);
		void draw(VkCommandBuffer commandBuffer, std::shared_ptr<Pipeline> pipeline, const std::vector<std::shared_ptr<DescriptorSet>> descriptorsets, bool isPresent);
	private:
		std::shared_ptr<Device> mDevice = nullptr;
		std::shared_ptr<CommandBuffer> mCommandBuffer = nullptr;
		std::shared_ptr<VertexBuffer> mVertexBuffer = nullptr;
		std::vector<Vertex> vertices;
	};

	//enum PrimitiveType {
	//	Triangle
	//};

	//class Primitive {
	//public:
	//	Primitive(PrimitiveType );
	//	void drawPrimitive(std::shared_ptr<Pipeline> pipeline);

	//	std::shared_ptr<VertexBuffer> mVertexBuffer = nullptr;
	//	std::shared_ptr<IndexBuffer> mIndexBuffer = nullptr;

	//	std::vector<Vertex> vertices;
	//	std::vector<u32> indices;
	//};

}