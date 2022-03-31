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
		Texture* baseColorTexture = nullptr;
		Texture* normalTexture = nullptr;
		Texture* metallicRoughnessTexture = nullptr;
		//Texture* occlusionTexture;
		//Texture* emissiveTexture;
		struct TexCoordSets {
			uint8_t baseColor = 0;
			uint8_t metallicRoughness = 0;
			uint8_t normal = 0;
			//uint8_t occlusion = 0;
			//uint8_t emissive = 0;
		} texCoordSets;

		DescriptorSet* materialDescriptorSet = nullptr;

		struct MaterialUbStruct {
			Math::vec4 baseColorFactor = Math::vec4(1.0f);
			Math::vec2 metallicRoughnessFactor = Math::vec2(1.0f);
		}materialUbStruct;
		UniformBuffer* materialUb;
	};
}