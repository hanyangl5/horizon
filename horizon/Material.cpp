#include "Material.h"

//void Material::setupDescriptorSet(Device* device)
//{
//	DescriptorSetInfo setInfo;
//	setInfo;
//	materialDescriptorSet = new DescriptorSet(device,&setInfo);
//}

void Material::initDescriptorSet()
{
}

void Material::updateDescriptorSet()
{
	materialUb->update(&materialUbStruct, sizeof(materialUbStruct));

	DescriptorSetUpdateDesc desc;
	desc.addBinding(0, materialUb);
	desc.addBinding(1, baseColorTexture);
	desc.addBinding(2, normalTexture);
	desc.addBinding(3, metallicRoughnessTexture);

	materialDescriptorSet->createDescriptorPool();
	materialDescriptorSet->allocateDescriptors();
	materialDescriptorSet->updateDescriptorSet(&desc);
}
