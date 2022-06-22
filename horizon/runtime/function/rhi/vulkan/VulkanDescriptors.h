#pragma once
#include <runtime/core/utils/definations.h>
#include <runtime/function/rhi/RHIUtils.h>
#include <array>

namespace Horizon {

	class VulkanDescriptor
	{
	public:
		VulkanDescriptor(VkDevice device) noexcept;
		~VulkanDescriptor() noexcept;
		void AllocateDescriptors() noexcept;

	private:
		VkDevice m_device;
		VkDescriptorPool m_bindless_descriptor_pool;
		std::vector<VkDescriptorSetLayout> m_set_layouts;
		std::vector<VkDescriptorSet> m_sets;
	};

	VulkanDescriptor::VulkanDescriptor(VkDevice device) noexcept : m_device(device)
	{

	}

	VulkanDescriptor::~VulkanDescriptor()
	{
	}
	inline void VulkanDescriptor::AllocateDescriptors() noexcept
	{

		// create descriptorpool
		if (m_bindless_descriptor_pool == VK_NULL_HANDLE) {
			std::vector<VkDescriptorPoolSize> pool_sizes = {
				{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1024 },
				{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1024 },
				{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1024 },
				{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1024 }
			};

			VkDescriptorPoolCreateInfo pool_info{};
			pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			pool_info.flags = 0;
			pool_info.poolSizeCount = static_cast<u32>(pool_sizes.size());
			pool_info.pPoolSizes = pool_sizes.data();
			pool_info.maxSets = static_cast<u32>(pool_sizes.size());

			CHECK_VK_RESULT(vkCreateDescriptorPool(m_device, &pool_info, nullptr, &m_bindless_descriptor_pool));
			
		}

		std::vector<VkDescriptorType> types{
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
		};

		VkDescriptorSetLayoutBinding binding{};
		binding.binding = 0;
		binding.descriptorCount = 1024;
		binding.stageFlags = VK_SHADER_STAGE_ALL;

		VkDescriptorBindingFlags flag = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;

		VkDescriptorSetLayoutBindingFlagsCreateInfo flag_info{};
		flag_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
		flag_info.bindingCount = 1;
		flag_info.pBindingFlags = &flag;

		VkDescriptorSetLayoutCreateInfo set_layout_info{};
		set_layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		set_layout_info.pNext = &flag_info;
		set_layout_info.bindingCount = 1;

		for (u32 i = 0; i < m_set_layouts.size(); i++) {
			binding.descriptorType = types[i];
			set_layout_info.pBindings = &binding;
			CHECK_VK_RESULT(vkCreateDescriptorSetLayout(m_device, &set_layout_info, nullptr, &m_set_layouts[i]));
		}

		VkDescriptorSetAllocateInfo set_info;
		set_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		set_info.descriptorPool = m_bindless_descriptor_pool;
		set_info.descriptorSetCount = 1;
		for (u32 i = 0; i < m_sets.size(); i++) {
			set_info.pSetLayouts = &m_set_layouts[i];
			CHECK_VK_RESULT(vkAllocateDescriptorSets(m_device, &set_info, &m_sets[i]));
		}


	}
}