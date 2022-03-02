#include "Model.h"
#include "VulkanBuffer.h"

Model::Model(const std::string& path, Device* device, CommandBuffer* command, DescriptorSet* descriptors) :mDevice(device), mCommandBuffer(command), sceneDescriptorSet(descriptors)
{
	
	tinygltf::TinyGLTF gltfContext;
	std::string error, warning;

	bool fileLoaded = gltfContext.LoadASCIIFromFile(&gltfModel, &error, &warning, path);
	if (fileLoaded) {
		loadTextures(gltfModel);
		loadMaterials(gltfModel);
		const tinygltf::Scene& scene = gltfModel.scenes[gltfModel.defaultScene > -1 ? gltfModel.defaultScene : 0];
		for (size_t i = 0; i < scene.nodes.size(); i++) {
			const tinygltf::Node node = gltfModel.nodes[scene.nodes[i]];
			float scale = 1.0;
			loadNode(nullptr, node, scene.nodes[i], gltfModel, indices, vertices, scale);
		}

		for (auto node : linearNodes) {
			// Initial pose of each node mesh
			if (node->mesh) {
				node->update();
			}
		}

		mVertexBuffer = new VertexBuffer(mDevice, mCommandBuffer, vertices);
		mIndexBuffer = new IndexBuffer(mDevice, mCommandBuffer, indices);
	}
}

Model::~Model() {
}

void Model::draw(Pipeline* pipeline)
{
	for (u32 i = 0; i < mCommandBuffer->commandBufferCount(); i++) {
		mCommandBuffer->beginRenderPass(i, pipeline);

		auto commandBuffer = *mCommandBuffer->get(i);
		//vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, 0, 0, nullptr, 0, nullptr, 0, nullptr);
		const VkDeviceSize offsets[1] = { 0 };
		VkBuffer vertexBuffer = mVertexBuffer->get();

		vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);
		vkCmdBindIndexBuffer(commandBuffer, mIndexBuffer->get(), 0, VK_INDEX_TYPE_UINT32);
		for (auto& node : nodes) {
			drawNode(node, pipeline, commandBuffer);
		}
		mCommandBuffer->endRenderPass(i);
	}

}

void Model::loadTextures(tinygltf::Model& gltfModel)
{
	auto getVkFilterMode = [](int32_t filterMode)
	{
		switch (filterMode) {
		case 9728:
			return VK_FILTER_NEAREST;
		case 9729:
			return VK_FILTER_LINEAR;
		case 9984:
			return VK_FILTER_NEAREST;
		case 9985:
			return VK_FILTER_NEAREST;
		case 9986:
			return VK_FILTER_LINEAR;
		case 9987:
			return VK_FILTER_LINEAR;
		}
	};

	auto getVkWrapMode = [](int32_t wrapMode)
	{
		switch (wrapMode) {
		case 10497:
			return VK_SAMPLER_ADDRESS_MODE_REPEAT;
		case 33071:
			return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		case 33648:
			return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
		}
	};

	std::vector<VkSamplerCreateInfo> samplers;

	for (tinygltf::Sampler smpl : gltfModel.samplers) {
		VkSamplerCreateInfo sampler{};
		sampler.minFilter = getVkFilterMode(smpl.minFilter);
		sampler.magFilter = getVkFilterMode(smpl.magFilter);
		sampler.addressModeU = getVkWrapMode(smpl.wrapS);
		sampler.addressModeV = getVkWrapMode(smpl.wrapT);
		sampler.addressModeW = sampler.addressModeV;
		samplers.push_back(sampler);
	}

	for (tinygltf::Texture& tex : gltfModel.textures) {
		tinygltf::Image image = gltfModel.images[tex.source];
		//VkSamplerCreateInfo samplerinfo;
		//if (tex.sampler == -1) {
		//	samplerinfo.magFilter = VK_FILTER_LINEAR;
		//	samplerinfo.minFilter = VK_FILTER_LINEAR;
		//	samplerinfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		//	samplerinfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		//	samplerinfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		//}
		//else {
		//	samplerinfo = samplers[tex.sampler];
		//}

		textures.emplace_back(Texture(mDevice, mCommandBuffer, image));
		;
	}
}

void Model::loadMaterials(tinygltf::Model& gltfModel)
{
	for (tinygltf::Material& mat : gltfModel.materials) {
		Material material{};

		// bc
		if (mat.values.find("baseColorTexture") != mat.values.end()) {
			material.baseColorTexture = &textures[mat.values["baseColorTexture"].TextureIndex()];
			material.texCoordSets.baseColor = mat.values["baseColorTexture"].TextureTexCoord();
		}
		if (mat.values.find("baseColorFactor") != mat.values.end()) {
			material.baseColorFactor = glm::make_vec4(mat.values["baseColorFactor"].ColorFactor().data());
		}

		// normal
		if (mat.additionalValues.find("normalTexture") != mat.additionalValues.end()) {
			material.normalTexture = &textures[mat.additionalValues["normalTexture"].TextureIndex()];
			material.texCoordSets.normal = mat.additionalValues["normalTexture"].TextureTexCoord();
		}

		// metallic roughtness
		if (mat.values.find("metallicRoughnessTexture") != mat.values.end()) {
			material.metallicRoughnessTexture = &textures[mat.values["metallicRoughnessTexture"].TextureIndex()];
			material.texCoordSets.metallicRoughness = mat.values["metallicRoughnessTexture"].TextureTexCoord();
		}
		if (mat.values.find("roughnessFactor") != mat.values.end()) {
			material.roughnessFactor = static_cast<float>(mat.values["roughnessFactor"].Factor());
		}
		if (mat.values.find("metallicFactor") != mat.values.end()) {
			material.metallicFactor = static_cast<float>(mat.values["metallicFactor"].Factor());
		}

		//if (mat.additionalValues.find("emissiveTexture") != mat.additionalValues.end()) {
		//	material.emissiveTexture = &textures[mat.additionalValues["emissiveTexture"].TextureIndex()];
		//	material.texCoordSets.emissive = mat.additionalValues["emissiveTexture"].TextureTexCoord();
		//}
		//if (mat.additionalValues.find("occlusionTexture") != mat.additionalValues.end()) {
		//	material.occlusionTexture = &textures[mat.additionalValues["occlusionTexture"].TextureIndex()];
		//	material.texCoordSets.occlusion = mat.additionalValues["occlusionTexture"].TextureTexCoord();
		//}
		//if (mat.additionalValues.find("emissiveFactor") != mat.additionalValues.end()) {
		//	material.emissiveFactor = glm::vec4(glm::make_vec3(mat.additionalValues["emissiveFactor"].ColorFactor().data()), 1.0);
		//	material.emissiveFactor = glm::vec4(0.0f);
		//}

		materials.push_back(material);
	}
	// Push a default material at the end of the list for meshes with no material assigned
	materials.push_back(Material());
}

void Model::loadNode(Node* parent, const tinygltf::Node& node, uint32_t nodeIndex, const tinygltf::Model& model, std::vector<u32>& indexBuffer, std::vector<Vertex>& vertexBuffer, float globalscale)
{
	Node* newNode = new Node{};
	newNode->index = nodeIndex;
	newNode->parent = parent;
	newNode->name = node.name;
	newNode->matrix = glm::mat4(1.0f);

	// Generate local node matrix
	glm::vec3 translation = glm::vec3(0.0f);
	if (node.translation.size() == 3) {
		translation = glm::make_vec3(node.translation.data());
		newNode->translation = translation;
	}
	glm::mat4 rotation = glm::mat4(1.0f);
	if (node.rotation.size() == 4) {
		glm::quat q = glm::make_quat(node.rotation.data());
		newNode->rotation = glm::mat4(q);
	}
	glm::vec3 scale = glm::vec3(1.0f);
	if (node.scale.size() == 3) {
		scale = glm::make_vec3(node.scale.data());
		newNode->scale = scale;
	}
	if (node.matrix.size() == 16) {
		newNode->matrix = glm::make_mat4x4(node.matrix.data());
	};

	// Node with children
	if (node.children.size() > 0) {
		for (size_t i = 0; i < node.children.size(); i++) {
			loadNode(newNode, model.nodes[node.children[i]], node.children[i], model, indexBuffer, vertexBuffer, globalscale);
		}
	}

	// Node contains mesh data
	if (node.mesh > -1) {
		const tinygltf::Mesh mesh = model.meshes[node.mesh];
		Mesh* newMesh = new Mesh(mDevice, newNode->matrix);
		for (size_t j = 0; j < mesh.primitives.size(); j++) {
			const tinygltf::Primitive& primitive = mesh.primitives[j];
			uint32_t indexStart = static_cast<uint32_t>(indexBuffer.size());
			uint32_t vertexStart = static_cast<uint32_t>(vertexBuffer.size());
			uint32_t indexCount = 0;
			uint32_t vertexCount = 0;
			glm::vec3 posMin{};
			glm::vec3 posMax{};
			bool hasSkin = false;
			bool hasIndices = primitive.indices > -1;
			// Vertices
			{
				const float* bufferPos = nullptr;
				const float* bufferNormals = nullptr;
				const float* bufferTexCoordSet0 = nullptr;
				const float* bufferTexCoordSet1 = nullptr;
				const void* bufferJoints = nullptr;
				const float* bufferWeights = nullptr;

				int posByteStride;
				int normByteStride;
				int uv0ByteStride;
				int uv1ByteStride;
				int jointByteStride;
				int weightByteStride;

				int jointComponentType;

				// Position attribute is required
				assert(primitive.attributes.find("POSITION") != primitive.attributes.end());

				const tinygltf::Accessor& posAccessor = model.accessors[primitive.attributes.find("POSITION")->second];
				const tinygltf::BufferView& posView = model.bufferViews[posAccessor.bufferView];
				bufferPos = reinterpret_cast<const float*>(&(model.buffers[posView.buffer].data[posAccessor.byteOffset + posView.byteOffset]));
				posMin = glm::vec3(posAccessor.minValues[0], posAccessor.minValues[1], posAccessor.minValues[2]);
				posMax = glm::vec3(posAccessor.maxValues[0], posAccessor.maxValues[1], posAccessor.maxValues[2]);
				vertexCount = static_cast<uint32_t>(posAccessor.count);
				posByteStride = posAccessor.ByteStride(posView) ? (posAccessor.ByteStride(posView) / sizeof(float)) : sizeof(glm::vec3) * 8;

				if (primitive.attributes.find("NORMAL") != primitive.attributes.end()) {
					const tinygltf::Accessor& normAccessor = model.accessors[primitive.attributes.find("NORMAL")->second];
					const tinygltf::BufferView& normView = model.bufferViews[normAccessor.bufferView];
					bufferNormals = reinterpret_cast<const float*>(&(model.buffers[normView.buffer].data[normAccessor.byteOffset + normView.byteOffset]));
					normByteStride = normAccessor.ByteStride(normView) ? (normAccessor.ByteStride(normView) / sizeof(float)) : sizeof(glm::vec3) * 8;
				}

				if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end()) {
					const tinygltf::Accessor& uvAccessor = model.accessors[primitive.attributes.find("TEXCOORD_0")->second];
					const tinygltf::BufferView& uvView = model.bufferViews[uvAccessor.bufferView];
					bufferTexCoordSet0 = reinterpret_cast<const float*>(&(model.buffers[uvView.buffer].data[uvAccessor.byteOffset + uvView.byteOffset]));
					uv0ByteStride = uvAccessor.ByteStride(uvView) ? (uvAccessor.ByteStride(uvView) / sizeof(float)) : sizeof(glm::vec3) * 8;
				}
				for (size_t v = 0; v < posAccessor.count; v++) {
					Vertex vert;
					vert.pos = glm::vec4(glm::make_vec3(&bufferPos[v * posByteStride]), 1.0f);
					vert.normal = glm::normalize(glm::vec3(bufferNormals ? glm::make_vec3(&bufferNormals[v * normByteStride]) : glm::vec3(0.0f)));
					vert.uv0 = bufferTexCoordSet0 ? glm::make_vec2(&bufferTexCoordSet0[v * uv0ByteStride]) : glm::vec3(0.0f);
					//vert.uv1 = bufferTexCoordSet1 ? glm::make_vec2(&bufferTexCoordSet1[v * uv1ByteStride]) : glm::vec3(0.0f);
					vertexBuffer.push_back(vert);
				}
			}
			// Indices
			if (hasIndices)
			{
				const tinygltf::Accessor& accessor = model.accessors[primitive.indices > -1 ? primitive.indices : 0];
				const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
				const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];

				indexCount = static_cast<uint32_t>(accessor.count);
				const void* dataPtr = &(buffer.data[accessor.byteOffset + bufferView.byteOffset]);

				switch (accessor.componentType) {
				case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT: {
					const uint32_t* buf = static_cast<const uint32_t*>(dataPtr);
					for (size_t index = 0; index < accessor.count; index++) {
						indexBuffer.push_back(buf[index] + vertexStart);
					}
					break;
				}
				case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT: {
					const uint16_t* buf = static_cast<const uint16_t*>(dataPtr);
					for (size_t index = 0; index < accessor.count; index++) {
						indexBuffer.push_back(buf[index] + vertexStart);
					}
					break;
				}
				case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE: {
					const uint8_t* buf = static_cast<const uint8_t*>(dataPtr);
					for (size_t index = 0; index < accessor.count; index++) {
						indexBuffer.push_back(buf[index] + vertexStart);
					}
					break;
				}
				default:
					//std::cerr << "Index component type " << accessor.componentType << " not supported!" << std::endl;
					return;
				}
			}
			Primitive* newPrimitive = new Primitive(indexStart, indexCount, vertexCount, primitive.material > -1 ? materials[primitive.material] : materials.back());
			//newPrimitive->setBoundingBox(posMin, posMax);
			newMesh->primitives.push_back(newPrimitive);
		}
		// Mesh BB from BBs of primitives

		newNode->mesh = newMesh;
	}
	if (parent) {
		parent->children.push_back(newNode);
	}
	else {
		nodes.push_back(newNode);
	}
	linearNodes.push_back(newNode);
}

void Model::drawNode(Node* node, Pipeline* pipeline, VkCommandBuffer commandBuffer)
{
	if (node->mesh) {
		for (Primitive* primitive : node->mesh->primitives) {
			//primitive->material;
			std::vector<VkDescriptorSet> descriptors{ sceneDescriptorSet->get(), node->mesh->meshDescriptorSet->get()};
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getLayout(), 0, descriptors.size(), descriptors.data(), 0, 0);
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->get());
			vkCmdDrawIndexed(commandBuffer, primitive->indexCount, 1, primitive->firstIndex, 0, 0);
		}
	}
	for (auto& child : node->children) {
		drawNode(child, pipeline, commandBuffer);
	}
}

void Model::updateDescriptors()
{
	for (auto& node : nodes) {
		if (node->mesh) {
			node->mesh->meshDescriptorSet->createDescriptorPool();
			node->mesh->meshDescriptorSet->allocateDescriptors();

			DescriptorSetUpdateDesc desc;
			desc.addBinding(0, node->mesh->modelMatrixUbo);

			node->mesh->meshDescriptorSet->updateDescriptorSet(&desc);
		}
		for (auto& child : node->children) {
			updateNodeDescriptorSet(child);
		}
	}

}

void Model::updateNodeDescriptorSet(Node* node) {

}

DescriptorSet* Model::getDescriptorSet()
{
	for (auto& node : nodes) {
		if (node->mesh) {
			return node->mesh->meshDescriptorSet;
		}
	}
	spdlog::error("model does not have descritporset");
	return nullptr;
}
