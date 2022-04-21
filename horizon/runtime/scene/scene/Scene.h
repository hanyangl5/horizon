#pragma once

#include <vector>
#include <unordered_map>

#include <runtime/function/render/RenderContext.h>
#include <runtime/scene/camera/Camera.h>
#include <runtime/function/render/rhi/Device.h>
#include <runtime/function/render/rhi/Descriptors.h>
#include <runtime/function/render/rhi/CommandBuffer.h>
#include <runtime/scene/model/Model.h>
#include <runtime/scene/light/Light.h>

namespace Horizon {

#define MAX_LIGHT_COUNT 1024

	class Scene {
	public:
		Scene(RenderContext& render_context, std::shared_ptr<Device> device, std::shared_ptr<CommandBuffer> command_buffer);
		~Scene();

		void LoadModel(const std::string& path, const std::string& name);
		std::shared_ptr<Model> GetModel(const std::string& name);
		
		// https://google.github.io/filament/Filament.html
		void AddDirectLight(Math::vec3 color, f32 intensity, Math::vec3 direction);
		void AddPointLight(Math::vec3 color, f32 intensity, Math::vec3 position, f32 radius);
		void AddSpotLight(Math::vec3 color, f32 intensity, Math::vec3 direction, Math::vec3 position, f32 radius, f32 innerConeAngle, f32 outerConeAngle);

		void Prepare();
		void Draw(u32 i, std::shared_ptr<CommandBuffer> command_buffer, std::shared_ptr<Pipeline> pipeline);
		std::shared_ptr<DescriptorSetLayouts> GetDescriptorLayouts();
		std::shared_ptr<DescriptorSetLayouts> GetGeometryPassDescriptorLayouts();
		std::shared_ptr<DescriptorSetLayouts> GetSceneDescriptorLayouts();
		std::shared_ptr<Camera> GetMainCamera() const;
		std::shared_ptr<UniformBuffer> getCameraUbo() const;
	private:
		RenderContext& m_render_context;
		std::shared_ptr<Camera> m_camera = nullptr;
		std::shared_ptr<Device> m_device;
		std::shared_ptr<CommandBuffer> m_command_buffer;
		std::shared_ptr<DescriptorSet> m_scene_descriptor_set = nullptr;

		// uniform buffers

		// 0
		struct SceneUb {
			Math::mat4 view;
			Math::mat4 projection;
			Math::vec2 nearFar;
		}m_scene_ubdata;
		std::shared_ptr<UniformBuffer> m_scene_ub = nullptr;
		// 1
		struct LightCountUb {
			u32 lightCount = 0;
		}m_light_count_ubdata;
		std::shared_ptr<UniformBuffer> m_light_count_ub;
		// 2
		struct LightsUb {
			LightParams lights[MAX_LIGHT_COUNT];
		}m_lights_ubdata;
		std::shared_ptr<UniformBuffer> m_light_ub;
		// 3
		struct CamaeraUb {
			Math::vec3 cameraPos;
		}m_camera_ubdata;
		std::shared_ptr<UniformBuffer> m_camera_ub;


		// models
		//std::vector<std::shared_ptr<Model>> m_models;
		std::unordered_map<std::string, std::shared_ptr<Model>> m_models;
	};

	class FullscreenTriangle {
	public:
		FullscreenTriangle(std::shared_ptr<Device> device, std::shared_ptr<CommandBuffer> command_buffer);
		void Draw(u32 _i, std::shared_ptr<CommandBuffer> _command_buffer, std::shared_ptr<Pipeline> _pipeline, const std::vector<std::shared_ptr<DescriptorSet>> _descriptor_sets, bool _is_present = false);
	private:
		std::shared_ptr<Device> m_device = nullptr;
		std::shared_ptr<CommandBuffer> m_command_buffer = nullptr;
		std::shared_ptr<VertexBuffer> m_vertex_buffer = nullptr;
		std::vector<Vertex> m_vertices;
	};

	//enum PrimitiveType {
	//	Triangle
	//};

	//class Primitive {
	//public:
	//	Primitive(PrimitiveType );
	//	void drawPrimitive(std::shared_ptr<Pipeline> pipeline);

	//	std::shared_ptr<VertexBuffer> m_vertex_buffer = nullptr;
	//	std::shared_ptr<IndexBuffer> m_index_buffer = nullptr;

	//	std::vector<Vertex> vertices;
	//	std::vector<u32> indices;
	//};

}