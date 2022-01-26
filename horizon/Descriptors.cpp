#include "Descriptors.h"
#include <unordered_map>
#include "UniformBuffer.h"
Descriptors::Descriptors(Device* device) :mDevice(device)
{
}

Descriptors::~Descriptors()
{
	vkDestroyDescriptorPool(mDevice->get(), mDescriptorPool, nullptr);
	for (auto& i : mSetLayouts) {
		vkDestroyDescriptorSetLayout(mDevice->get(), i, nullptr);
	}

}

void Descriptors::createDescriptorSetLayout()
{
	mSetLayouts.resize(mDescriptorSetInfos.size());
	for (u32 set = 0; set < mDescriptorSetInfos.size(); set++) {
		std::vector<VkDescriptorSetLayoutBinding> bindings(mDescriptorSetInfos[set].bindingCount);
		for (u32 binding = 0; binding < mDescriptorSetInfos[set].bindingCount; binding++) {
			bindings[binding].binding = binding;
			bindings[binding].descriptorType = mDescriptorSetInfos[set].types[binding];
			bindings[binding].descriptorCount = 1;
			bindings[binding].stageFlags = mDescriptorSetInfos[set].stageFlags[binding];
		}
		VkDescriptorSetLayoutCreateInfo layoutInfo{};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = bindings.size();
		layoutInfo.pBindings = bindings.data();

		if (vkCreateDescriptorSetLayout(mDevice->get(), &layoutInfo, nullptr, &mSetLayouts[set]) != VK_SUCCESS) {
			throw std::runtime_error("failed to create descriptor set layout!");
		}
	}
}
void Descriptors::addDescriptorSet(DescriptorSetInfo* setInfo) {
	mDescriptorSetInfos.push_back(*setInfo);
}

void Descriptors::updateDescriptorSet(u32 setIndex, BufferDesc* desc)
{
	// update descriptor set
	std::vector<VkWriteDescriptorSet> descriptorWrites(mDescriptorSetInfos[setIndex].bindingCount);
	for (u32 binding = 0; binding < mDescriptorSetInfos[setIndex].bindingCount; binding++) {
		descriptorWrites[binding].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[binding].pNext=nullptr;
		descriptorWrites[binding].dstSet = mSets[setIndex];
		descriptorWrites[binding].dstBinding = binding;
		descriptorWrites[binding].dstArrayElement = 0;
		descriptorWrites[binding].descriptorCount = 1;
		descriptorWrites[binding].descriptorType = mDescriptorSetInfos[setIndex].types[binding];
		switch (descriptorWrites[binding].descriptorType)
		{
		case VK_DESCRIPTOR_TYPE_SAMPLER:
			//static_cast<VkDescriptorImageInfo>(desc[binding].bufferData);
			VkDescriptorImageInfo imageInfo;
			descriptorWrites[binding].pImageInfo = &imageInfo;
			break;
		case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
		case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
			//VkDescriptorImageInfo imageInfo;
			descriptorWrites[binding].pImageInfo = &imageInfo;
			break;
		case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
		case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
			VkBufferView texelBufferView;
			descriptorWrites[binding].pTexelBufferView = &texelBufferView;
			break;
		case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
		case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
			VkDescriptorBufferInfo bufferInfo;
			bufferInfo.buffer = desc->ubos[binding]->get();
			bufferInfo.offset = 0;
			bufferInfo.range = desc->ubos[binding]->size();
			descriptorWrites[binding].pBufferInfo = &bufferInfo;
			break;
		default:
			break;
		}

		vkUpdateDescriptorSets(mDevice->get(), descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
	}
}
u32 Descriptors::getSetCount() const
{
	return mSets.size();
}

VkDescriptorSetLayout* Descriptors::getLayouts() {
	return mSetLayouts.data();
}
VkDescriptorSet* Descriptors::get()
{
	return mSets.data();
}
void Descriptors::allocateDescriptors() {
	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = mDescriptorPool;
	allocInfo.descriptorSetCount = mDescriptorSetInfos.size();
	allocInfo.pSetLayouts = mSetLayouts.data();
	mSets.resize(mDescriptorSetInfos.size());
	printVkError(vkAllocateDescriptorSets(mDevice->get(), &allocInfo, mSets.data()), "create descriptor sets", logLevel::debug);
}

void Descriptors::createDescriptorPool() {
	std::unordered_map<VkDescriptorType, u32> descriptorTypeMap;

	// calculate each descriptor type count
	for (u32 set = 0; set < mDescriptorSetInfos.size(); set++) {
		for (u32 binding = 0; binding < mDescriptorSetInfos[set].bindingCount; binding++) {
			descriptorTypeMap[mDescriptorSetInfos[set].types[binding]]++;
		}
	}
	std::vector< VkDescriptorPoolSize> poolSizes{};
	for (auto& type : descriptorTypeMap) {
		poolSizes.push_back({ type.first,1 });
	}

	VkDescriptorPoolCreateInfo poolInfo{};

	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.pNext = nullptr;
	poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
	poolInfo.maxSets = 1 * poolSizes.size();
	poolInfo.poolSizeCount = poolSizes.size();
	poolInfo.pPoolSizes = poolSizes.data();

	printVkError(vkCreateDescriptorPool(mDevice->get(), &poolInfo, nullptr, &mDescriptorPool), "create descriptor pool", logLevel::debug);
}

void DescriptorSetInfo::addBinding(VkDescriptorType type, VkShaderStageFlags stage)
{
	bindingCount++;
	types.push_back(type);
	stageFlags.push_back(stage);
}
