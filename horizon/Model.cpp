#include "Model.h"

#include "VulkanBuffer.h"

namespace Horizon {
	Model::Model(const std::string& path, std::shared_ptr<Device> device, std::shared_ptr<CommandBuffer> commandBuffer, std::shared_ptr<DescriptorSet> sceneDescritporSet) :mDevice(device), mCommandBuffer(commandBuffer), sceneDescriptorSet(sceneDescritporSet)
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

			mVertexBuffer = std::make_shared<VertexBuffer>(mDevice, mCommandBuffer, vertices);
			mIndexBuffer = std::make_shared<IndexBuffer>(mDevice, mCommandBuffer, indices);
		}
		else
		{
			spdlog::error("failed to load file {} {}", error, warning);
		}
	}

	Model::~Model() {

	}

	void Model::draw(std::shared_ptr<Pipeline> pipeline, VkCommandBuffer commandBuffer)
	{
		const VkDeviceSize offsets[1] = { 0 };
		VkBuffer vertexBuffer = mVertexBuffer->get();

		vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);
		vkCmdBindIndexBuffer(commandBuffer, mIndexBuffer->get(), 0, VK_INDEX_TYPE_UINT32);
		for (auto& node : nodes) {
			drawNode(node, pipeline, commandBuffer);
		}
	}

	void Model::loadTextures(tinygltf::Model& gltfModel)
	{
		//auto getVkFilterMode = [](int32_t filterMode)
		//{
		//	switch (filterMode) {
		//	case 9728:
		//		return VK_FILTER_NEAREST;
		//	case 9729:
		//		return VK_FILTER_LINEAR;
		//	case 9984:
		//		return VK_FILTER_NEAREST;
		//	case 9985:
		//		return VK_FILTER_NEAREST;
		//	case 9986:
		//		return VK_FILTER_LINEAR;
		//	case 9987:
		//		return VK_FILTER_LINEAR;
		//	default:
		//		return VK_FILTER_NEAREST;
		//	}
		//};

		//auto getVkWrapMode = [](int32_t wrapMode)
		//{
		//	switch (wrapMode) {
		//	case 10497:
		//		return VK_SAMPLER_ADDRESS_MODE_REPEAT;
		//	case 33071:
		//		return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		//	case 33648:
		//		return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
		//	default:
		//		return VK_SAMPLER_ADDRESS_MODE_REPEAT;
		//	}
		//};

		//std::vector<VkSamplerCreateInfo> samplers;

		//for (tinygltf::Sampler smpl : gltfModel.samplers) {
		//	VkSamplerCreateInfo sampler{};
		//	sampler.minFilter = getVkFilterMode(smpl.minFilter);
		//	sampler.magFilter = getVkFilterMode(smpl.magFilter);
		//	sampler.addressModeU = getVkWrapMode(smpl.wrapS);
		//	sampler.addressModeV = getVkWrapMode(smpl.wrapT);
		//	sampler.addressModeW = sampler.addressModeV;
		//	samplers.push_back(sampler);
		//}

		for (tinygltf::Texture& tex : gltfModel.textures) {
			//auto& image = gltfModel.images[tex.source];
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

			textures.emplace_back(std::make_shared<Texture>(mDevice, mCommandBuffer, gltfModel.images[tex.source]));

		}

		if (!mEmptyTexture) {
			mEmptyTexture = std::make_shared<Texture>(mDevice, mCommandBuffer);
			//mEmptyTexture->loadFromFile("C:/Users/hylu/OneDrive/mycode/vulkan/data/black.bmp", VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		}
	}

	void Model::loadMaterials(tinygltf::Model& gltfModel)
	{
		for (tinygltf::Material& mat : gltfModel.materials) {
			std::shared_ptr<Material> material = std::make_shared<Material>();
			// bc
			if (mat.values.find("baseColorTexture") != mat.values.end()) {
				material->baseColorTexture = textures[mat.values["baseColorTexture"].TextureIndex()];
				material->texCoordSets.baseColor = mat.values["baseColorTexture"].TextureTexCoord();
			}
			else {
				spdlog::warn("no base color texture found, use an empty texture instead");
				material->baseColorTexture = mEmptyTexture;
			}

			//if (mat.values.find("baseColorFactor") != mat.values.end()) {
			//	material.materialUbStruct.baseColorFactor = Math::make_vec4(mat.values["baseColorFactor"].ColorFactor().data());
			//} else{
			//	spdlog::warn("no base color factor found, use default param instead");
			//}

			// normal
			if (mat.additionalValues.find("normalTexture") != mat.additionalValues.end()) {
				material->normalTexture = textures[mat.additionalValues["normalTexture"].TextureIndex()];
				material->texCoordSets.normal = mat.additionalValues["normalTexture"].TextureTexCoord();
			}
			else {
				spdlog::warn("no normal texture found, use an empty texture instead");
				material->normalTexture = mEmptyTexture;
			}

			// metallic roughtness
			if (mat.values.find("metallicRoughnessTexture") != mat.values.end()) {
				material->metallicRoughnessTexture = textures[mat.values["metallicRoughnessTexture"].TextureIndex()];
				material->texCoordSets.metallicRoughness = mat.values["metallicRoughnessTexture"].TextureTexCoord();
			}
			else {
				spdlog::warn("no normal texture found, use an empty texture instead");
				material->metallicRoughnessTexture = mEmptyTexture;
			}
			/*
			if (mat.values.find("roughnessFactor") != mat.values.end()) {
				material.materialUbStruct.metallicRoughnessFactor.y = static_cast<float>(mat.values["roughnessFactor"].Factor());
			} else{
				spdlog::warn("no roughnessFactor found, use default param instead");
			}
			if (mat.values.find("metallicFactor") != mat.values.end()) {
				material.materialUbStruct.metallicRoughnessFactor.x = static_cast<float>(mat.values["metallicFactor"].Factor());
			} else{
				spdlog::warn("no metallicFactor found, use default param instead");
			}*/

			//if (mat.additionalValues.find("emissiveTexture") != mat.additionalValues.end()) {
			//	material.emissiveTexture = &textures[mat.additionalValues["emissiveTexture"].TextureIndex()];
			//	material.texCoordSets.emissive = mat.additionalValues["emissiveTexture"].TextureTexCoord();
			//}
			//if (mat.additionalValues.find("occlusionTexture") != mat.additionalValues.end()) {
			//	material.occlusionTexture = &textures[mat.additionalValues["occlusionTexture"].TextureIndex()];
			//	material.texCoordSets.occlusion = mat.additionalValues["occlusionTexture"].TextureTexCoord();
			//}
			//if (mat.additionalValues.find("emissiveFactor") != mat.additionalValues.end()) {
			//	material.emissiveFactor = Math::vec4(Math::make_vec3(mat.additionalValues["emissiveFactor"].ColorFactor().data()), 1.0);
			//	material.emissiveFactor = Math::vec4(0.0f);
			//}

			material->materialUb = std::make_shared<UniformBuffer>(mDevice);


			std::shared_ptr<DescriptorSetInfo> setInfo = std::make_shared<DescriptorSetInfo>();
			// material parameters
			setInfo->addBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
			// albedo/normal/metallicroughness
			setInfo->addBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
			setInfo->addBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
			setInfo->addBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
			material->materialDescriptorSet = std::make_shared<DescriptorSet>(mDevice, setInfo);

			materials.push_back(material);

		}
		// Push a default material at the end of the list for meshes with no material assigned
		//materials.push_back(Material());
	}

	void Model::loadNode(std::shared_ptr<Node> parent, const tinygltf::Node& node, uint32_t nodeIndex, const tinygltf::Model& model, std::vector<u32>& indices, std::vector<Vertex>& vertices, float globalscale)
	{
		std::shared_ptr<Node> newNode = std::make_shared<Node>();
		newNode->index = nodeIndex;
		newNode->parent = parent;
		newNode->name = node.name;
		newNode->matrix = Math::mat4(1.0f);

		// Generate local node matrix
		Math::vec3 translation = Math::vec3(0.0f);
		if (node.translation.size() == 3) {
			translation = Math::make_vec3(node.translation.data());
			newNode->translation = translation;
		}
		if (node.rotation.size() == 4) {
			newNode->rotation = Math::make_quat(node.rotation.data());
		}
		Math::vec3 scale = Math::vec3(1.0f);
		if (node.scale.size() == 3) {
			scale = Math::make_vec3(node.scale.data());
			newNode->scale = scale;
		}
		if (node.matrix.size() == 16) {
			newNode->matrix = Math::make_mat4x4(node.matrix.data());
		};

		// Node with children
		if (node.children.size() > 0) {
			for (size_t i = 0; i < node.children.size(); i++) {
				loadNode(newNode, model.nodes[node.children[i]], node.children[i], model, indices, vertices, globalscale);
			}
		}

		// Node contains mesh data
		if (node.mesh > -1) {
			const tinygltf::Mesh mesh = model.meshes[node.mesh];
			std::shared_ptr<Mesh> newMesh = std::make_shared<Mesh>(mDevice, newNode->matrix);
			for (size_t j = 0; j < mesh.primitives.size(); j++) {
				const tinygltf::Primitive& primitive = mesh.primitives[j];
				uint32_t indexStart = static_cast<uint32_t>(indices.size());
				uint32_t vertexStart = static_cast<uint32_t>(vertices.size());
				uint32_t indexCount = 0;
				uint32_t vertexCount = 0;
				Math::vec3 posMin{};
				Math::vec3 posMax{};
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
					posMin = Math::vec3(posAccessor.minValues[0], posAccessor.minValues[1], posAccessor.minValues[2]);
					posMax = Math::vec3(posAccessor.maxValues[0], posAccessor.maxValues[1], posAccessor.maxValues[2]);
					vertexCount = static_cast<uint32_t>(posAccessor.count);
					posByteStride = posAccessor.ByteStride(posView) ? (posAccessor.ByteStride(posView) / sizeof(float)) : sizeof(Math::vec3) * 8;

					if (primitive.attributes.find("NORMAL") != primitive.attributes.end()) {
						const tinygltf::Accessor& normAccessor = model.accessors[primitive.attributes.find("NORMAL")->second];
						const tinygltf::BufferView& normView = model.bufferViews[normAccessor.bufferView];
						bufferNormals = reinterpret_cast<const float*>(&(model.buffers[normView.buffer].data[normAccessor.byteOffset + normView.byteOffset]));
						normByteStride = normAccessor.ByteStride(normView) ? (normAccessor.ByteStride(normView) / sizeof(float)) : sizeof(Math::vec3) * 8;
					}

					if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end()) {
						const tinygltf::Accessor& uvAccessor = model.accessors[primitive.attributes.find("TEXCOORD_0")->second];
						const tinygltf::BufferView& uvView = model.bufferViews[uvAccessor.bufferView];
						bufferTexCoordSet0 = reinterpret_cast<const float*>(&(model.buffers[uvView.buffer].data[uvAccessor.byteOffset + uvView.byteOffset]));
						uv0ByteStride = uvAccessor.ByteStride(uvView) ? (uvAccessor.ByteStride(uvView) / sizeof(float)) : sizeof(Math::vec3) * 8;
					}
					for (size_t v = 0; v < posAccessor.count; v++) {
						Vertex vert;
						vert.pos = Math::make_vec3(&bufferPos[v * posByteStride]);
						vert.normal = Math::normalize(Math::vec3(bufferNormals ? Math::make_vec3(&bufferNormals[v * normByteStride]) : Math::vec3(0.0f)));
						vert.uv0 = bufferTexCoordSet0 ? Math::make_vec2(&bufferTexCoordSet0[v * uv0ByteStride]) : Math::vec3(0.0f);
						//vert.uv1 = bufferTexCoordSet1 ? Math::make_vec2(&bufferTexCoordSet1[v * uv1ByteStride]) : Math::vec3(0.0f);
						vertices.push_back(vert);
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
							indices.push_back(buf[index] + vertexStart);
						}
						break;
					}
					case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT: {
						const uint16_t* buf = static_cast<const uint16_t*>(dataPtr);
						for (size_t index = 0; index < accessor.count; index++) {
							indices.push_back(buf[index] + vertexStart);
						}
						break;
					}
					case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE: {
						const uint8_t* buf = static_cast<const uint8_t*>(dataPtr);
						for (size_t index = 0; index < accessor.count; index++) {
							indices.push_back(buf[index] + vertexStart);
						}
						break;
					}
					default:
						//std::cerr << "Index component type " << accessor.componentType << " not supported!" << std::endl;
						return;
					}
				}
				newMesh->primitives.emplace_back(std::make_shared<Primitive>(indexStart, indexCount, vertexCount, primitive.material > -1 ? materials[primitive.material] : materials[0]));
			}
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

	void Model::drawNode(std::shared_ptr<Node> node, std::shared_ptr<Pipeline> pipeline, VkCommandBuffer commandBuffer)
	{
		if (node->mesh) {
			for (auto& primitive : node->mesh->primitives) {
				std::vector<VkDescriptorSet> descriptors{ sceneDescriptorSet->get(),  primitive->material->materialDescriptorSet->get(), node->mesh->meshDescriptorSet->get() };
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
			updateNodeDescriptorSet(node);
		}
	}

	void Model::updateNodeDescriptorSet(std::shared_ptr<Node> node) {
		if (node->mesh) {
			node->mesh->meshDescriptorSet->createDescriptorPool();
			node->mesh->meshDescriptorSet->allocateDescriptors();

			DescriptorSetUpdateDesc desc;
			desc.addBinding(0, node->mesh->meshUb);

			node->mesh->meshDescriptorSet->updateDescriptorSet(desc);
			// update material params and textures

			for (auto& primitive : node->mesh->primitives) {
				primitive->material->updateDescriptorSet();
			}
		}
		for (auto& child : node->children) {
			updateNodeDescriptorSet(child);
		}
	}

	std::shared_ptr<DescriptorSet> Model::getMeshDescriptorSet()
	{
		for (auto& node : nodes) {
			return getNodeMeshDescriptorSet(node);
		}
		spdlog::error("mesh descriptorset not found");
		return nullptr;
	}
	std::shared_ptr<DescriptorSet> Model::getNodeMeshDescriptorSet(std::shared_ptr<Node> node)
	{
		if (node->mesh) {
			return node->mesh->meshDescriptorSet;
		}
		for (auto& child : node->children) {
			return getNodeMeshDescriptorSet(child);
		}
		return nullptr;
	}
	std::shared_ptr<DescriptorSet> Model::getMaterialDescriptorSet()
	{
		for (auto& node : nodes) {
			return getNodeMaterialDescriptorSet(node);
		}
		spdlog::error("material descriptorset not found");
		return nullptr;
	}
	std::shared_ptr<DescriptorSet> Model::getNodeMaterialDescriptorSet(std::shared_ptr<Node> node)
	{
		if (node->mesh) {
			for (auto& primitive : node->mesh->primitives) {
				return primitive->material->materialDescriptorSet;
			}
		}
		for (auto& child : node->children) {
			return getNodeMaterialDescriptorSet(child);
		}
		return nullptr;
	}


	Mesh::Mesh(std::shared_ptr<Device> device, Math::mat4 model) :mDevice(device), meshUbStruct({ model })
	{

		meshUb = std::make_shared<UniformBuffer>(mDevice);
		std::shared_ptr<DescriptorSetInfo> setInfo = std::make_shared<DescriptorSetInfo>();
		setInfo->addBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
		meshDescriptorSet = std::make_shared<DescriptorSet>(mDevice, setInfo);

	}

	Mesh::~Mesh()
	{

	}

	Primitive::Primitive(uint32_t firstIndex, uint32_t indexCount, uint32_t vertexCount, std::shared_ptr<Material> material) : firstIndex(firstIndex), indexCount(indexCount), vertexCount(vertexCount), material(material) {
		hasIndices = indexCount > 0;
	}

	Node::Node() {

	}

	Node::~Node() {
	}


	Math::mat4 Node::localMatrix() {
		return Math::translate(Math::mat4(1.0f), translation) * Math::mat4_cast(rotation) * Math::scale(Math::mat4(1.0f), scale) * matrix;
	}

	Math::mat4 Node::getMatrix() {
		Math::mat4 m = localMatrix();
		std::shared_ptr<Node> p = parent;
		while (p) {
			m = p->localMatrix() * m;
			p = p->parent;
		}
		return m;
	}
	void Node::update() {
		if (mesh) {
			Math::mat4 m = getMatrix();
			mesh->meshUbStruct.model = m;
			mesh->meshUb->update(&m, sizeof(mesh->meshUbStruct));
		}

		for (auto& child : children) {
			child->update();
		}
	}
}