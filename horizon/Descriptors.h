#pragma once
#include <vulkan/vulkan.hpp>
#include <vector>
#include "utils.h"
#include "Device.h"

struct DescriptorSetUpdateDesc
{
	// sampler/Ub/sbo
public:
	void addBinding(u32 binding, DescriptorBase* buffer);
	std::unordered_map<u32, DescriptorBase*> descriptorMap;
};

struct DescriptorSetInfo {
	u32 bindingCount = 0;
	std::vector<VkDescriptorType> types{};
	std::vector<VkShaderStageFlags> stageFlags{};
	void addBinding(VkDescriptorType type, VkShaderStageFlags stage);
};

class DescriptorSet
{
public:
	/*DescriptorSet(Device* device);*/
	DescriptorSet(Device* device, DescriptorSetInfo* setInfo);
	~DescriptorSet();
	VkDescriptorSetLayout getLayout();
	VkDescriptorSet get();
	void createDescriptorSetLayout();
	void allocateDescriptors();
	void createDescriptorPool();
	void updateDescriptorSet(DescriptorSetUpdateDesc* desc);

private:
	Device* mDevice = nullptr;
	DescriptorSetInfo mDescriptorSetInfo;
	VkDescriptorSetLayout mSetLayout;
	VkDescriptorSet mSet;
	VkDescriptorPool mDescriptorPool;
};
