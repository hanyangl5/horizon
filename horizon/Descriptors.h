#pragma once
#include <vulkan/vulkan.hpp>
#include <vector>
#include "utils.h"
#include "Device.h"

class UniformBuffer;

struct BufferDesc
{
	//void* bufferData;
	std::vector<UniformBuffer*> ubos;
};

struct DescriptorSetInfo {
	u32 bindingCount = 0;
	std::vector<VkDescriptorType> types{};
	std::vector<VkShaderStageFlags> stageFlags{};
	void addBinding(VkDescriptorType type, VkShaderStageFlags stage);
};

class Descriptors
{
public:
	Descriptors(Device* device);
	~Descriptors();
	VkDescriptorSetLayout* getLayouts();
	VkDescriptorSet* get();
	void createDescriptorSetLayout();
	void allocateDescriptors();
	void createDescriptorPool();
	void addDescriptorSet(DescriptorSetInfo* setInfo);
	void updateDescriptorSet(u32 setIndex, BufferDesc* desc);
	u32 getSetCount() const;
private:
	Device* mDevice = nullptr;
	std::vector<DescriptorSetInfo> mDescriptorSetInfos;
	std::vector<VkDescriptorSetLayout> mSetLayouts;
	std::vector<VkDescriptorSet> mSets;
	VkDescriptorPool mDescriptorPool;
};
