#include "utils.h"
#include "Texture.h"
#include "UniformBuffer.h"
#include "Descriptors.h"
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>

class Material {
public:
	void initDescriptorSet();
	void updateDescriptorSet();

	//glm::vec4 emissiveFactor = glm::vec4(1.0f);
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

	//void setupDescriptorSet(Device* device);

	DescriptorSet* materialDescriptorSet;

	struct MaterialUboStruct {
		glm::vec4 baseColorFactor = glm::vec4(1.0f);
		glm::vec2 metallicRoughnessFactor = glm::vec2(1.0f);
	}materialUboStruct;
	UniformBuffer* materialUbo;
};