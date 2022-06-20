#pragma once
#include <runtime/core/utils/definations.h>
#include <runtime/function/rhi/RHIUtils.h>
#include <array>
class VulkanDescriptor
{
public:
	VulkanDescriptor();
	~VulkanDescriptor();

private:

};

VulkanDescriptor::VulkanDescriptor()
{
	std::vector<VkDescriptorPoolSize> pool_sizes{};
	pool_sizes.emplace_back(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1024);
	pool_sizes.emplace_back(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1024);
	pool_sizes.emplace_back(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1024);
	pool_sizes.emplace_back(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1024);

	VkDescriptorPoolCreateInfo pool_info{};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.poolSizeCount = static_cast<u32>(pool_sizes.size());
	pool_info.pPoolSizes = pool_sizes.data();
	pool_info.maxSets = 3;

	//VK_CHECK(vkCreateDescriptorPool(device, &pool_info, nullptr, &descriptor_pool));
}

VulkanDescriptor::~VulkanDescriptor()
{
}