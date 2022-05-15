#pragma once

#include <memory>
#include <unordered_map>
#include <vector>

#include <runtime/function/render/RenderContext.h>
#include <runtime/function/render/rhi/VulkanEnums.h>
#include "Device.h"

namespace Horizon {

	struct DescriptorSetUpdateDesc
	{
		// sampler/Ub/sbo
	public:
		void BindResource(u32 binding, std::shared_ptr<DescriptorBase> buffer);
		std::unordered_map<u32, std::shared_ptr<DescriptorBase>> descriptorMap;
	};

	struct DescriptorSetInfo {
		u32 bindingCount = 0;
		std::vector<DescriptorType> types{};
		std::vector<u32> stageFlags{};
		void AddBinding(DescriptorType type, u32 stage);
	};

	struct DescriptorSetLayouts {
		std::vector<VkDescriptorSetLayout> layouts;
	};

	class DescriptorSet
	{
	public:
		DescriptorSet(std::shared_ptr<Device> device, std::shared_ptr<DescriptorSetInfo> setInfo);
		~DescriptorSet();
		VkDescriptorSetLayout GetLayout();
		VkDescriptorSet Get();
		void AllocateDescriptorSet();
		void UpdateDescriptorSet(const DescriptorSetUpdateDesc& desc);
	private:
		void CreateDescriptorSetLayout();
		void CreateDescriptorPool();
	private:
		std::shared_ptr<Device> m_device = nullptr;
		std::shared_ptr<DescriptorSetInfo> mDescriptorSetInfo;
		VkDescriptorSetLayout mSetLayout = VK_NULL_HANDLE;
		VkDescriptorSet mSet = VK_NULL_HANDLE;
		VkDescriptorPool mDescriptorPool = VK_NULL_HANDLE;
	};


}