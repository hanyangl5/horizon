#include "Model.h"

#include <runtime/core/log/Log.h>
#include <runtime/core/path/Path.h>
#include <runtime/function/rhi/vulkan/VulkanBuffer.h>

namespace Horizon {
	Model::Model(const std::string& path, std::shared_ptr<Device> device, std::shared_ptr<CommandBuffer> command_buffer, std::shared_ptr<DescriptorSet> m_scene_descriptor_set) noexcept :m_device(device), m_command_buffer(command_buffer), m_scene_descriptor_set(m_scene_descriptor_set)
	{

		tinygltf::TinyGLTF gltf_context;
		std::string error, warning;

		tinygltf::Model gltf_model;

		bool file_loaded = gltf_context.LoadASCIIFromFile(&gltf_model, &error, &warning, path);
		if (file_loaded) {
			LoadTextures(gltf_model);
			LoadMaterials(gltf_model);
			const tinygltf::Scene& scene = gltf_model.scenes[gltf_model.defaultScene > -1 ? gltf_model.defaultScene : 0];
			for (size_t i = 0; i < scene.nodes.size(); i++) {
				const tinygltf::Node node = gltf_model.nodes[scene.nodes[i]];
				f32 scale = 1.0;
				LoadNode(nullptr, node, scene.nodes[i], gltf_model, m_indices, m_vertices, scale);
			}


			m_vertex_buffer = std::make_shared<VertexBuffer>(m_device, m_command_buffer, m_vertices);
			m_index_buffer = std::make_shared<IndexBuffer>(m_device, m_command_buffer, m_indices);
		}
		else
		{
			LOG_ERROR("{} {}", error, warning);
		}
	}

	Model::~Model() noexcept {

	}

	void Model::Draw(std::shared_ptr<Pipeline> pipeline, VkCommandBuffer command_buffer) noexcept
	{
		const VkDeviceSize offsets[1] = { 0 };
		VkBuffer vertexBuffer = m_vertex_buffer->Get();

		vkCmdBindVertexBuffers(command_buffer, 0, 1, &vertexBuffer, offsets);
		vkCmdBindIndexBuffer(command_buffer, m_index_buffer->Get(), 0, VK_INDEX_TYPE_UINT32);
		for (auto& node : m_nodes) {
			DrawNode(node, pipeline, command_buffer);
		}
	}

	void Model::LoadTextures(tinygltf::Model& gltfModel) noexcept
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

			m_textures.emplace_back(std::make_shared<Texture>(m_device, m_command_buffer, gltfModel.images[tex.source]));

		}

		if (!m_empty_texture) {
			m_empty_texture = std::make_shared<Texture>(m_device, m_command_buffer);
			m_empty_texture->loadFromFile(Path::GetInstance().GetTexturePath("black.jpg"), VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		}
	}

	void Model::LoadMaterials(tinygltf::Model& gltfModel) noexcept
	{
		for (tinygltf::Material& mat : gltfModel.materials) {
			std::shared_ptr<Material> material = std::make_shared<Material>();
			// bc
			if (mat.values.find("baseColorTexture") != mat.values.end()) {
				material->base_color_texture = m_textures[mat.values["baseColorTexture"].TextureIndex()];
				//material->texCoordSets.baseColor = mat.values["base_color_texture"].TextureTexCoord();
				material->m_material_ubdata.has_base_color = true;
			}
			else {
				LOG_WARN("no base color texture found, use an empty texture instead");
				material->base_color_texture = m_empty_texture;
			}

			//if (mat.values.find("baseColorFactor") != mat.values.end()) {
			//	material.m_material_ubdata.baseColorFactor = Math::make_vec4(mat.values["baseColorFactor"].ColorFactor().data());
			//} else{
			//	LOG_WARN("no base color factor found, use default param instead");
			//}

			// normal
			if (mat.additionalValues.find("normalTexture") != mat.additionalValues.end()) {
				material->normal_texture = m_textures[mat.additionalValues["normalTexture"].TextureIndex()];
				//material->texCoordSets.normal = mat.additionalValues["normal_texture"].TextureTexCoord();
				material->m_material_ubdata.has_normal = true;
			}
			else {
				LOG_WARN("no normal texture found, use an empty texture instead");
				material->normal_texture = m_empty_texture;
			}

			// metallic roughtness
			if (mat.values.find("metallicRoughnessTexture") != mat.values.end()) {
				material->metallic_rougness_texture = m_textures[mat.values["metallicRoughnessTexture"].TextureIndex()];
				//material->texCoordSets.metallicRoughness = mat.values["metallic_rougness_texture"].TextureTexCoord();
				material->m_material_ubdata.has_metallic_rougness = true;
			}
			else {
				LOG_WARN("no metallicRoughness texture found, use an empty texture instead");
				material->metallic_rougness_texture = m_empty_texture;
			}
			/*
			if (mat.values.find("roughnessFactor") != mat.values.end()) {
				material.m_material_ubdata.metallicRoughnessFactor.y = static_cast<f32>(mat.values["roughnessFactor"].Factor());
			} else{
				LOG_WARN("no roughnessFactor found, use default param instead");
			}
			if (mat.values.find("metallicFactor") != mat.values.end()) {
				material.m_material_ubdata.metallicRoughnessFactor.x = static_cast<f32>(mat.values["metallicFactor"].Factor());
			} else{
				LOG_WARN("no metallicFactor found, use default param instead");
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

			material->m_material_ub = std::make_shared<UniformBuffer>(m_device);


			std::shared_ptr<DescriptorSetInfo> setInfo = std::make_shared<DescriptorSetInfo>();
			// material parameters
			setInfo->AddBinding(DescriptorType::DESCRIPTOR_TYPE_UNIFORM_BUFFER, SHADER_STAGE_VERTEX_SHADER | SHADER_STAGE_PIXEL_SHADER);
			// albedo/normal/metallicroughness
			setInfo->AddBinding(DescriptorType::DESCRIPTOR_TYPE_TEXTURE, SHADER_STAGE_VERTEX_SHADER | SHADER_STAGE_PIXEL_SHADER);
			setInfo->AddBinding(DescriptorType::DESCRIPTOR_TYPE_TEXTURE, SHADER_STAGE_VERTEX_SHADER | SHADER_STAGE_PIXEL_SHADER);
			setInfo->AddBinding(DescriptorType::DESCRIPTOR_TYPE_TEXTURE, SHADER_STAGE_VERTEX_SHADER | SHADER_STAGE_PIXEL_SHADER);
			material->m_material_descriptor_set = std::make_shared<DescriptorSet>(m_device, setInfo);

			m_materials.push_back(material);

		}
		// Push a default material at the end of the list for meshes with no material assigned
		//m_materials.push_back(Material());
	}

	void Model::LoadNode(std::shared_ptr<Node> m_parent, const tinygltf::Node& node, uint32_t nodeIndex, const tinygltf::Model& model, std::vector<u32>& indices, std::vector<Vertex>& vertices, f32 globalscale) noexcept
	{
		std::shared_ptr<Node> newNode = std::make_shared<Node>();
		newNode->index = nodeIndex;
		newNode->m_parent = m_parent;
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

		// Node with m_children
		if (node.children.size() > 0) {
			for (size_t i = 0; i < node.children.size(); i++) {
				LoadNode(newNode, model.nodes[node.children[i]], node.children[i], model, indices, vertices, globalscale);
			}
		}

		// Node contains mesh data
		if (node.mesh > -1) {
			const tinygltf::Mesh mesh = model.meshes[node.mesh];
			std::shared_ptr<Mesh> newMesh = std::make_shared<Mesh>(m_device, newNode->matrix);
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
					const f32* bufferPos = nullptr;
					const f32* bufferNormals = nullptr;
					const f32* bufferTexCoordSet0 = nullptr;
					const f32* bufferTexCoordSet1 = nullptr;
					const void* bufferJoints = nullptr;
					const f32* bufferWeights = nullptr;

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
					bufferPos = reinterpret_cast<const f32*>(&(model.buffers[posView.buffer].data[posAccessor.byteOffset + posView.byteOffset]));
					posMin = Math::vec3(posAccessor.minValues[0], posAccessor.minValues[1], posAccessor.minValues[2]);
					posMax = Math::vec3(posAccessor.maxValues[0], posAccessor.maxValues[1], posAccessor.maxValues[2]);
					vertexCount = static_cast<uint32_t>(posAccessor.count);
					posByteStride = posAccessor.ByteStride(posView) ? (posAccessor.ByteStride(posView) / sizeof(f32)) : sizeof(Math::vec3) * 8;

					if (primitive.attributes.find("NORMAL") != primitive.attributes.end()) {
						const tinygltf::Accessor& normAccessor = model.accessors[primitive.attributes.find("NORMAL")->second];
						const tinygltf::BufferView& normView = model.bufferViews[normAccessor.bufferView];
						bufferNormals = reinterpret_cast<const f32*>(&(model.buffers[normView.buffer].data[normAccessor.byteOffset + normView.byteOffset]));
						normByteStride = normAccessor.ByteStride(normView) ? (normAccessor.ByteStride(normView) / sizeof(f32)) : sizeof(Math::vec3) * 8;
					}

					if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end()) {
						const tinygltf::Accessor& uvAccessor = model.accessors[primitive.attributes.find("TEXCOORD_0")->second];
						const tinygltf::BufferView& uvView = model.bufferViews[uvAccessor.bufferView];
						bufferTexCoordSet0 = reinterpret_cast<const f32*>(&(model.buffers[uvView.buffer].data[uvAccessor.byteOffset + uvView.byteOffset]));
						uv0ByteStride = uvAccessor.ByteStride(uvView) ? (uvAccessor.ByteStride(uvView) / sizeof(f32)) : sizeof(Math::vec3) * 8;
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
				newMesh->primitives.emplace_back(std::make_shared<MeshPrimitive>(indexStart, indexCount, vertexCount, primitive.material > -1 ? m_materials[primitive.material] : m_materials[0]));
			}
			newNode->mesh = newMesh;
		}
		if (m_parent) {
			m_parent->m_children.push_back(newNode);
		}
		else {
			m_nodes.push_back(newNode);
		}
		m_linear_nodes.push_back(newNode);
	}

	void Model::DrawNode(std::shared_ptr<Node> node, std::shared_ptr<Pipeline> pipeline, VkCommandBuffer command_buffer) noexcept
	{
		if (node->mesh) {
			for (auto& primitive : node->mesh->primitives) {
				std::vector<VkDescriptorSet> descriptors{ m_scene_descriptor_set->Get(),  primitive->material->m_material_descriptor_set->Get() };

				vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetLayout(), 0, descriptors.size(), descriptors.data(), 0, 0);
				vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->Get());
				if (pipeline->hasPushConstants()) {
					vkCmdPushConstants(command_buffer, pipeline->GetLayout(), SHADER_STAGE_VERTEX_SHADER, 0, sizeof(node->mesh->m_mesh_push_constant), &node->mesh->m_mesh_push_constant);
				}
				vkCmdDrawIndexed(command_buffer, primitive->indexCount, 1, primitive->firstIndex, 0, 0);
			}
		}
		for (auto& child : node->m_children) {
			DrawNode(child, pipeline, command_buffer);
		}
	}

	void Model::UpdateDescriptors() noexcept
	{
		for (auto& material : m_materials) {
			material->UpdateDescriptorSet();
		}
	}

	void Model::UpdateModelMatrix() noexcept
	{
		for (auto& node : m_nodes) {
			UpdateNodeModelMatrix(node);
		}
	}

	void Model::UpdateNodeModelMatrix(std::shared_ptr<Node> node) noexcept
	{

		// update model matrix for each node
		if (node->mesh) {
			node->update(m_model_matrix);
		}
		for (auto& child : node->m_children) {
			UpdateNodeModelMatrix(child);
		}
	}


	//std::shared_ptr<DescriptorSet> Model::getMeshDescriptorSet()
	//{
	//	for (auto& node : nodes) {
	//		return getNodeMeshDescriptorSet(node);
	//	}
	//	LOG_ERROR("mesh descriptorset not found");
	//	return nullptr;
	//}
	//std::shared_ptr<DescriptorSet> Model::getNodeMeshDescriptorSet(std::shared_ptr<Node> node)
	//{
	//	if (node->mesh) {
	//		return node->mesh->meshDescriptorSet;
	//	}
	//	for (auto& child : node->m_children) {
	//		return getNodeMeshDescriptorSet(child);
	//	}
	//	return nullptr;
	//}
	std::shared_ptr<DescriptorSet> Model::GetMaterialDescriptorSet() noexcept
	{
		for (auto& node : m_nodes) {
			return GetNodeMaterialDescriptorSet(node);
		}
		LOG_ERROR("material descriptorset not found");
		return nullptr;
	}

	void Model::SetModelMatrix(const Math::mat4& modelMatrix) noexcept
	{
		m_model_matrix = modelMatrix;
	}

	std::shared_ptr<DescriptorSet> Model::GetNodeMaterialDescriptorSet(std::shared_ptr<Node> node) noexcept
	{
		if (node->mesh) {
			for (auto& primitive : node->mesh->primitives) {
				return primitive->material->m_material_descriptor_set;
			}
		}
		for (auto& child : node->m_children) {
			return GetNodeMaterialDescriptorSet(child);
		}
		return nullptr;
	}


	Mesh::Mesh(std::shared_ptr<Device> device, Math::mat4 model) noexcept :m_device(device)
	{
		m_mesh_push_constant.modelMatrix = model;
		//meshUb = std::make_shared<UniformBuffer>(m_device);
		//std::shared_ptr<DescriptorSetInfo> setInfo = std::make_shared<DescriptorSetInfo>();
		//setInfo->AddBinding(DESCRIPTOR_TYPE_UNIFORM_BUFFER, SHADER_STAGE_VERTEX_SHADER);
		//meshDescriptorSet = std::make_shared<DescriptorSet>(m_device, setInfo);
        }
        
	MeshPrimitive::MeshPrimitive(uint32_t firstIndex, uint32_t indexCount, uint32_t vertexCount, std::shared_ptr<Material> material) noexcept : firstIndex(firstIndex), indexCount(indexCount), vertexCount(vertexCount), material(material) {
		hasIndices = indexCount > 0;
	}

	Node::Node() noexcept {

	}

	Node::~Node() noexcept {
	}

	Math::mat4 Node::localMatrix() noexcept {
		return Math::translate(Math::mat4(1.0f), translation) * Math::mat4_cast(rotation) * Math::scale(Math::mat4(1.0f), scale) * matrix;
	}

	Math::mat4 Node::getMatrix() noexcept {
		// local matrix
		Math::mat4 m = localMatrix();
		std::shared_ptr<Node> p = m_parent;
		while (p) {
			m = p->localMatrix() * m;
			p = p->m_parent;
		}
		return m;
	}

	void Node::update(const Math::mat4& modelMat) noexcept {
		mesh->m_mesh_push_constant.modelMatrix = modelMat * getMatrix();
		//mesh->meshUbStruct.model = modelMat * getMatrix();
		//mesh->meshUb->update(&mesh->meshUbStruct, sizeof(mesh->meshUbStruct));

		for (auto& child : m_children) {
			child->update(modelMat);
		}
	}
}