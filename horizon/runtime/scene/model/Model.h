#pragma once

#include <vector>

#include <vulkan/vulkan.hpp>
#include "tiny_gltf.h"

#include <runtime/function/render/rhi/Device.h>
#include <runtime/function/render/rhi/CommandBuffer.h>
#include <runtime/function/render/rhi/Descriptors.h>
#include <runtime/function/render/rhi/Pipeline.h>
#include <runtime/function/render/rhi/UniformBuffer.h>
#include <runtime/function/render/rhi/VertexBuffer.h>
#include <runtime/function/render/rhi/IndexBuffer.h>
#include <runtime/function/render/rhi/Texture.h>
#include <runtime/scene/material/Material.h>


namespace Horizon {

	class MeshPrimitive {
	public:
		MeshPrimitive(uint32_t firstIndex, uint32_t indexCount, uint32_t vertexCount, std::shared_ptr<Material> material);
	public:
		std::shared_ptr<Material> material;
		uint32_t firstIndex;
		uint32_t indexCount;
		uint32_t vertexCount;
		bool hasIndices;
	};

	class Mesh {
	public:
		Mesh(std::shared_ptr<Device> device, Math::mat4 model);
		~Mesh();

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
		Node();
		~Node();
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
		Math::mat4 localMatrix();
		Math::mat4 getMatrix();
		void update(const Math::mat4& modelMat);
	};


	class Model {
	public:
		Model(const std::string& path, std::shared_ptr<Device> device, std::shared_ptr<CommandBuffer> command_buffer, std::shared_ptr<DescriptorSet> m_scene_descriptor_set);
		~Model();
		void draw(std::shared_ptr<Pipeline> pipeline, VkCommandBuffer command_buffer);
		void loadTextures(tinygltf::Model& gltfModel);
		void loadMaterials(tinygltf::Model& gltfModel);
		void loadNode(std::shared_ptr<Node> m_parent, const tinygltf::Node& node, uint32_t nodeIndex, const tinygltf::Model& model, std::vector<u32>& indexBuffer, std::vector<Vertex>& vertexBuffer, f32 globalscale);
		void drawNode(std::shared_ptr<Node> node, std::shared_ptr<Pipeline> pipeline, VkCommandBuffer command_buffer);
		void updateDescriptors();
		void updateModelMatrix();
		//std::shared_ptr<DescriptorSet> getMeshDescriptorSet();
		std::shared_ptr<DescriptorSet> getMaterialDescriptorSet();
		void setModelMatrix(const Math::mat4& modelMatrix);
	private:
		void updateNodeModelMatrix(std::shared_ptr<Node> node);
		//void updateNodeDescriptorSet(std::shared_ptr<Node> node);
		//std::shared_ptr<DescriptorSet> getNodeMeshDescriptorSet(std::shared_ptr<Node> node);
		std::shared_ptr<DescriptorSet> getNodeMaterialDescriptorSet(std::shared_ptr<Node> node);
	private:
		std::shared_ptr<Device> m_device;
		std::shared_ptr<CommandBuffer> m_command_buffer;

		std::shared_ptr<DescriptorSet> m_scene_descriptor_set;

		tinygltf::Model gltfModel;

		Math::mat4 mModelMatrix = Math::mat4(1.0);

		std::shared_ptr<VertexBuffer> m_vertex_buffer = nullptr;
		std::shared_ptr<IndexBuffer> m_index_buffer = nullptr;

		std::vector<Vertex> vertices;
		std::vector<u32> indices;

		std::vector<std::shared_ptr<Node>> nodes;
		std::vector<std::shared_ptr<Node>> linearNodes;

		std::vector<std::shared_ptr<Texture>> textures;
		// TODO: empty texture only need to create once
		std::shared_ptr<Texture> mEmptyTexture = nullptr;
		std::vector<std::shared_ptr<Material>> materials;
	};
}