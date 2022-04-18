#pragma once

#include <memory>

#include <runtime/function/render/rhi/Texture.h>
#include <runtime/function/render/rhi/UniformBuffer.h>
#include <runtime/function/render/rhi/Descriptors.h>

namespace Horizon {

	class Material {
	public:
		void UpdateDescriptorSet();

		//Math::vec4 emissiveFactor = Math::vec4(1.0f);
		std::shared_ptr<Texture> base_color_texture = nullptr;
		std::shared_ptr<Texture> normal_texture = nullptr;
		std::shared_ptr<Texture> metallic_rougness_texture = nullptr;
		//std::shared_ptr<Texture> occlusionTexture;
		//std::shared_ptr<Texture> emissiveTexture;
		// struct TexCoordSets {
		// 	uint8_t baseColor = 0;
		// 	uint8_t metallicRoughness = 0;
		// 	uint8_t normal = 0;
		// 	//uint8_t occlusion = 0;
		// 	//uint8_t emissive = 0;
		// } texCoordSets;

		std::shared_ptr<DescriptorSet> m_material_descriptor_set = nullptr;

		struct MaterialUb {
			bool has_base_color = false;
			bool has_normal = false;
			bool has_metallic_rougness = false;
			//Math::vec4 baseColorFactor = Math::vec4(0.0f);
			//Math::vec3 normalFactor = Math::vec3(0.0f);
			//Math::vec2 metallicRoughnessFactor = Math::vec2(0.0f);
		}m_material_ubdata;
		std::shared_ptr<UniformBuffer> m_material_ub;
	};
}