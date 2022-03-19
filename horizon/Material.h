#pragma once

#include "utils.h"
#include "Texture.h"
#include "UniformBuffer.h"
#include "Descriptors.h"

namespace Horizon {

	class Material {
	public:
		void updateDescriptorSet();

		//vec4 emissiveFactor = vec4(1.0f);
		Texture* baseColorTexture;
		Texture* normalTexture;
		Texture* metallicRoughnessTexture;
		//Texture* occlusionTexture;
		//Texture* emissiveTexture;
		struct TexCoordSets {
			uint8_t baseColor = 0;
			uint8_t metallicRoughness = 0;
			uint8_t normal = 0;
			//uint8_t occlusion = 0;
			//uint8_t emissive = 0;
		} texCoordSets;

		DescriptorSet* materialDescriptorSet;

		struct MaterialUbStruct {
			vec4 baseColorFactor = vec4(1.0f);
			vec2 metallicRoughnessFactor = vec2(1.0f);
		}materialUbStruct;
		UniformBuffer* materialUb;
	};
}