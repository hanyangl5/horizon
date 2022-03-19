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

	class Primitive {
	public:
		Primitive(uint32_t firstIndex, uint32_t indexCount, uint32_t vertexCount, Material& material);
		uint32_t firstIndex;
		uint32_t indexCount;
		uint32_t vertexCount;
		Material& material;
		bool hasIndices;

	};

	class Mesh {
	public:
		Mesh(Device* device, mat4 model);
		~Mesh();

		Device* mDevice;;
		std::vector<Primitive*> primitives;

		struct MeshUbStruct {
			mat4 model;
		}meshUbStruct;
		UniformBuffer* meshUb = nullptr;
		DescriptorSet* meshDescriptorSet = nullptr;
	};

	class Node {
	public:
		Node();
		~Node();
		Node* parent;
		uint32_t index;
		std::vector<Node*> children;
		mat4 matrix;
		std::string name;
		Mesh* mesh;
		int32_t skinIndex = -1;
		vec3 translation{};
		vec3 scale{ 1.0f };
		quat rotation{};
		mat4 localMatrix();
		mat4 getMatrix();
		void update();

	};


	class Model {
	public:
		Model(const std::string& path, Device* device, CommandBuffer* commandBuffer, DescriptorSet* sceneDescritporSet);
		~Model();
		void draw(Pipeline* pipeline);
		void loadTextures(tinygltf::Model& gltfModel);
		void loadMaterials(tinygltf::Model& gltfModel);
		void loadNode(Node* parent, const tinygltf::Node& node, uint32_t nodeIndex, const tinygltf::Model& model, std::vector<u32>& indexBuffer, std::vector<Vertex>& vertexBuffer, float globalscale);
		void drawNode(Node* node, Pipeline* pipeline, VkCommandBuffer commandBuffer);
		void updateDescriptors();
		void updateNodeDescriptorSet(Node* node);
		DescriptorSet* getMeshDescriptorSet();
		DescriptorSet* getMaterialDescriptorSet();

	private:
		Device* mDevice;
		CommandBuffer* mCommandBuffer;

		DescriptorSet* sceneDescriptorSet;

		tinygltf::Model gltfModel;

		VertexBuffer* mVertexBuffer = nullptr;
		IndexBuffer* mIndexBuffer = nullptr;
		std::vector<Vertex> vertices;
		std::vector<u32> indices;

		std::vector<Node*> nodes;
		std::vector<Node*> linearNodes;

		std::vector<Texture> textures;
		//std::vector<TextureSampler> textureSamplers;
		std::vector<Material> materials;
	};
}