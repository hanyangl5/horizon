#pragma once

#include "utils.h"
#include "Texture.h"
#include "UniformBuffer.h"
#include "Descriptors.h"

namespace Horizon {

	class Material {
	public:
		void updateDescriptorSet();

		//Math::vec4 emissiveFactor = Math::vec4(1.0f);
		std::shared_ptr<Texture> baseColorTexture = nullptr;
		std::shared_ptr<Texture> normalTexture = nullptr;
		std::shared_ptr<Texture> metallicRoughnessTexture = nullptr;
		//std::shared_ptr<Texture> occlusionTexture;
		//std::shared_ptr<Texture> emissiveTexture;
		struct TexCoordSets {
			uint8_t baseColor = 0;
			uint8_t metallicRoughness = 0;
			uint8_t normal = 0;
			//uint8_t occlusion = 0;
			//uint8_t emissive = 0;
		} texCoordSets;

		std::shared_ptr<DescriptorSet> materialDescriptorSet = nullptr;

		struct MaterialUbStruct {
			Math::vec4 baseColorFactor = Math::vec4(1.0f);
			Math::vec2 metallicRoughnessFactor = Math::vec2(1.0f);
		}materialUbStruct;
		std::shared_ptr<UniformBuffer> materialUb;
	};
}