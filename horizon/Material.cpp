#include "Material.h"

namespace Horizon {

	void Material::updateDescriptorSet()
	{
		materialUb->update(&materialUbStruct, sizeof(materialUbStruct));

		DescriptorSetUpdateDesc desc;
		desc.addBinding(0, materialUb);
		desc.addBinding(1, baseColorTexture);
		desc.addBinding(2, normalTexture);
		desc.addBinding(3, metallicRoughnessTexture);

		materialDescriptorSet->allocateDescriptorSet();
		materialDescriptorSet->updateDescriptorSet(desc);
	}

}