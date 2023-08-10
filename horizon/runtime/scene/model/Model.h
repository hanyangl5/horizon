#pragma once

#include <vector>

#include <vulkan/vulkan.hpp>

#include <tiny_gltf.h>

#include <runtime/function/rhi/vulkan/Device.h>
#include <runtime/function/rhi/vulkan/CommandBuffer.h>
#include <runtime/function/rhi/vulkan/Descriptors.h>
#include <runtime/function/rhi/vulkan/Pipeline.h>
#include <runtime/function/rhi/vulkan/UniformBuffer.h>
#include <runtime/function/rhi/vulkan/VertexBuffer.h>
#include <runtime/function/rhi/vulkan/IndexBuffer.h>
#include <runtime/function/rhi/vulkan/Texture.h>
#include <runtime/scene/material/Material.h>


namespace Horizon {

	class MeshPrimitive {
	public:
		MeshPrimitive(uint32_t firstIndex, uint32_t indexCount, uint32_t vertexCount, std::shared_ptr<Material> material) noexcept;
	public:
		std::shared_ptr<Material> material;
		uint32_t firstIndex;
		uint32_t indexCount;
		uint32_t vertexCount;
		bool hasIndices;
	};

	class Mesh {
	public:
		Mesh(std::shared_ptr<Device> device, Math::mat4 model) noexcept;
		~Mesh() noexcept = default;

		std::shared_ptr<Device> m_device;;
		std::vector<std::shared_ptr<MeshPrimitive>> primitives;


		// 128 bytes push constant
		struct MeshPushConstant {
			Math::mat4 modelMatrix;
			Math::mat4 padding;
		}m_mesh_push_constant;

		//std::shared_ptr<UniformBuffer> meshUb = nullptr;
		//std::shared_ptr<DescriptorSet> meshDescriptorSet = nullptr;
	};

	class Node {
	public:
		Node() noexcept;
		~Node() noexcept;
		std::shared_ptr<Node> m_parent;
		uint32_t index;
		std::vector<std::shared_ptr<Node>> m_children;
		Math::mat4 matrix;
		std::string name;
		std::shared_ptr<Mesh> mesh = nullptr;
		int32_t skinIndex = -1;
		Math::vec3 translation{};
		Math::vec3 scale{ 1.0f };
		Math::quat rotation{};
		Math::mat4 localMatrix() noexcept;
		Math::mat4 getMatrix() noexcept;
		void update(const Math::mat4& modelMat) noexcept;
	};


	class Model {
	public:
		Model(const std::string& path, std::shared_ptr<Device> device, std::shared_ptr<CommandBuffer> command_buffer, std::shared_ptr<DescriptorSet> m_scene_descriptor_set) noexcept;
		~Model() noexcept;
		void Draw(std::shared_ptr<Pipeline> pipeline, VkCommandBuffer command_buffer) noexcept;
		void LoadTextures(tinygltf::Model& gltfModel) noexcept;
		void LoadMaterials(tinygltf::Model& gltfModel) noexcept;
		void LoadNode(std::shared_ptr<Node> m_parent, const tinygltf::Node& node, uint32_t nodeIndex, const tinygltf::Model& model, std::vector<u32>& indexBuffer, std::vector<Vertex>& vertexBuffer, f32 globalscale) noexcept;
		void DrawNode(std::shared_ptr<Node> node, std::shared_ptr<Pipeline> pipeline, VkCommandBuffer command_buffer) noexcept;
		void UpdateDescriptors() noexcept;
		void UpdateModelMatrix() noexcept;
		//std::shared_ptr<DescriptorSet> getMeshDescriptorSet();
		std::shared_ptr<DescriptorSet> GetMaterialDescriptorSet() noexcept;
		void SetModelMatrix(const Math::mat4& modelMatrix) noexcept;
	private:
		void UpdateNodeModelMatrix(std::shared_ptr<Node> node) noexcept;
		//void updateNodeDescriptorSet(std::shared_ptr<Node> node);
		//std::shared_ptr<DescriptorSet> getNodeMeshDescriptorSet(std::shared_ptr<Node> node);
		std::shared_ptr<DescriptorSet> GetNodeMaterialDescriptorSet(std::shared_ptr<Node> node) noexcept;
	private:
		std::shared_ptr<Device> m_device;
		std::shared_ptr<CommandBuffer> m_command_buffer;

		std::shared_ptr<DescriptorSet> m_scene_descriptor_set;


		Math::mat4 m_model_matrix = Math::mat4(1.0);

		std::shared_ptr<VertexBuffer> m_vertex_buffer = nullptr;
		std::shared_ptr<IndexBuffer> m_index_buffer = nullptr;

		std::vector<Vertex> m_vertices;
		std::vector<u32> m_indices;

		std::vector<std::shared_ptr<Node>> m_nodes;
		std::vector<std::shared_ptr<Node>> m_linear_nodes;

		std::vector<std::shared_ptr<Texture>> m_textures;
		std::vector<std::shared_ptr<Material>> m_materials;

		// TODO: empty texture only need to create once
		std::shared_ptr<Texture> m_empty_texture = nullptr;
	};
}