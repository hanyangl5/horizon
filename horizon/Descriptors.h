#pragma once

#include <unordered_map>
#include <vector>

#include <vulkan/vulkan.hpp>

#include "utils.h"
#include "Device.h"

namespace Horizon {

	struct DescriptorSetUpdateDesc
	{
		// sampler/Ub/sbo
	public:
		void addBinding(u32 binding, std::shared_ptr<DescriptorBase> buffer);
		std::unordered_map<u32, std::shared_ptr<DescriptorBase>> descriptorMap;
	};

	struct DescriptorSetInfo {
		u32 bindingCount = 0;
		std::vector<VkDescriptorType> types{};
		std::vector<VkShaderStageFlags> stageFlags{};
		void addBinding(VkDescriptorType type, VkShaderStageFlags stage);
	};

	struct DescriptorSetLayouts {
		std::vector<VkDescriptorSetLayout> layouts;
	};

	class DescriptorSet
	{
	public:
		DescriptorSet(std::shared_ptr<Device> device, std::shared_ptr<DescriptorSetInfo> setInfo);
		~DescriptorSet();
		VkDescriptorSetLayout getLayout();
		VkDescriptorSet get();
		void createDescriptorSetLayout();
		void allocateDescriptorSet();
		void createDescriptorPool();
		void updateDescriptorSet(const DescriptorSetUpdateDesc& desc);
	private:
		std::shared_ptr<Device> mDevice = nullptr;
		std::shared_ptr<DescriptorSetInfo> mDescriptorSetInfo;
		VkDescriptorSetLayout mSetLayout = VK_NULL_HANDLE;
		VkDescriptorSet mSet = VK_NULL_HANDLE;
		VkDescriptorPool mDescriptorPool = VK_NULL_HANDLE;
	};


}