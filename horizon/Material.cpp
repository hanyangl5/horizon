#include "Material.h"

namespace Horizon {

	void Material::updateDescriptorSet()
	{
		materialUb->update(&materialUbStruct, sizeof(materialUbStruct));

		DescriptorSetUpdateDesc desc;
		desc.bindResource(0, materialUb);
		desc.bindResource(1, baseColorTexture);
		desc.bindResource(2, normalTexture);
		desc.bindResource(3, metallicRoughnessTexture);

		materialDescriptorSet->allocateDescriptorSet();
		materialDescriptorSet->updateDescriptorSet(desc);
	}

}