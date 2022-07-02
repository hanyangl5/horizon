#pragma once
#include <runtime/core/utils/Definations.h>
#include <runtime/function/rhi/RHIUtils.h>
#include <array>

namespace Horizon::RHI {

	class VulkanDescriptor
	{
	public:
		VulkanDescriptor(VkDevice device) noexcept;
		~VulkanDescriptor() noexcept;
		void AllocateDescriptors() noexcept;
		void ResetDescriptorPool() noexcept;
	public:
		VkDevice m_device;
		VkDescriptorPool m_bindless_descriptor_pool;
		std::vector<VkDescriptorSetLayout> m_set_layouts;
		std::vector<VkDescriptorSet> m_sets;
	};

}