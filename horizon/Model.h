#pragma once

#include <vector>

#include <vulkan/vulkan.hpp>
#include "tiny_gltf.h"

#include "Device.h"
#include "CommandBuffer.h"
#include "Descriptors.h"
#include "Pipeline.h"
#include "UniformBuffer.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "Texture.h"
#include "Material.h"

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

		std::shared_ptr<Device> mDevice;;
		std::vector<std::shared_ptr<MeshPrimitive>> primitives;

		struct MeshUbStruct {
			Math::mat4 model;
		}meshUbStruct;
		std::shared_ptr<UniformBuffer> meshUb = nullptr;
		std::shared_ptr<DescriptorSet> meshDescriptorSet = nullptr;
	};

	class Node {
	public:
		Node();
		~Node();
		std::shared_ptr<Node> parent;
		uint32_t index;
		std::vector<std::shared_ptr<Node>> children;
		Math::mat4 matrix;
		std::string name;
		std::shared_ptr<Mesh> mesh = nullptr;
		int32_t skinIndex = -1;
		Math::vec3 translation{};
		Math::vec3 scale{ 1.0f };
		Math::quat rotation{};
		Math::mat4 localMatrix();
		Math::mat4 getMatrix();
		void update();
	};


	class Model {
	public:
		Model(const std::string& path, std::shared_ptr<Device> device, std::shared_ptr<CommandBuffer> commandBuffer, std::shared_ptr<DescriptorSet> sceneDescritporSet);
		~Model();
		void draw(std::shared_ptr<Pipeline> pipeline, VkCommandBuffer commandBuffer);
		void loadTextures(tinygltf::Model& gltfModel);
		void loadMaterials(tinygltf::Model& gltfModel);
		void loadNode(std::shared_ptr<Node> parent, const tinygltf::Node& node, uint32_t nodeIndex, const tinygltf::Model& model, std::vector<u32>& indexBuffer, std::vector<Vertex>& vertexBuffer, float globalscale);
		void drawNode(std::shared_ptr<Node> node, std::shared_ptr<Pipeline> pipeline, VkCommandBuffer commandBuffer);
		void updateDescriptors();
		void updateNodeDescriptorSet(std::shared_ptr<Node> node);
		std::shared_ptr<DescriptorSet> getMeshDescriptorSet();
		std::shared_ptr<DescriptorSet> getMaterialDescriptorSet();
	private:
		std::shared_ptr<DescriptorSet> getNodeMeshDescriptorSet(std::shared_ptr<Node> node);
		std::shared_ptr<DescriptorSet> getNodeMaterialDescriptorSet(std::shared_ptr<Node> node);
	private:
		std::shared_ptr<Device> mDevice;
		std::shared_ptr<CommandBuffer> mCommandBuffer;

		std::shared_ptr<DescriptorSet> sceneDescriptorSet;

		tinygltf::Model gltfModel;

		std::shared_ptr<VertexBuffer> mVertexBuffer = nullptr;
		std::shared_ptr<IndexBuffer> mIndexBuffer = nullptr;

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