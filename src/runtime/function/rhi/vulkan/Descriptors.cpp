#include "Descriptors.h"

#include <unordered_map>

#include <runtime/core/log/Log.h>

#include "UniformBuffer.h"

namespace Horizon {

DescriptorSet::DescriptorSet(std::shared_ptr<Device> device, std::shared_ptr<DescriptorSetInfo> setInfo)
    : m_device(device), mDescriptorSetInfo(setInfo) {
    CreateDescriptorSetLayout();
    CreateDescriptorPool();
}

DescriptorSet::~DescriptorSet() {
    vkDestroyDescriptorPool(m_device->Get(), mDescriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(m_device->Get(), mSetLayout, nullptr);
}

void DescriptorSet::CreateDescriptorSetLayout() {

    std::vector<VkDescriptorSetLayoutBinding> bindings(mDescriptorSetInfo->bindingCount);
    for (u32 binding = 0; binding < mDescriptorSetInfo->bindingCount; binding++) {
        bindings[binding].binding = binding;
        bindings[binding].descriptorType = ToVkDescriptorType(mDescriptorSetInfo->types[binding]);
        bindings[binding].descriptorCount = 1;
        bindings[binding].stageFlags = ToVkShaderStageFlags(mDescriptorSetInfo->stageFlags[binding]);
    }
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = bindings.size();
    layoutInfo.pBindings = bindings.data();

    CHECK_VK_RESULT(vkCreateDescriptorSetLayout(m_device->Get(), &layoutInfo, nullptr, &mSetLayout));
}

void DescriptorSet::UpdateDescriptorSet(const DescriptorSetUpdateDesc &desc) {
    AllocateDescriptorSet();
    // update descriptor set
    std::vector<VkWriteDescriptorSet> descriptorWrites(mDescriptorSetInfo->bindingCount);
    for (u32 binding = 0; binding < mDescriptorSetInfo->bindingCount; binding++) {
        descriptorWrites[binding].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[binding].pNext = nullptr;
        descriptorWrites[binding].dstSet = mSet;
        descriptorWrites[binding].dstBinding = binding;
        descriptorWrites[binding].dstArrayElement = 0;
        descriptorWrites[binding].descriptorCount = 1;
        descriptorWrites[binding].descriptorType = ToVkDescriptorType(mDescriptorSetInfo->types[binding]);

        switch (descriptorWrites[binding].descriptorType) {
        case VK_DESCRIPTOR_TYPE_SAMPLER:
        case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
        case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
            descriptorWrites[binding].pImageInfo = &desc.descriptorMap.at(binding).get()->imageDescriptorInfo;
            break;
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            descriptorWrites[binding].pBufferInfo = &desc.descriptorMap.at(binding).get()->bufferDescriptrInfo;
            break;
        default:
            break;
        }
    }
    vkUpdateDescriptorSets(m_device->Get(), descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
}

VkDescriptorSetLayout DescriptorSet::GetLayout() { return mSetLayout; }

VkDescriptorSet DescriptorSet::Get() { return mSet; }

void DescriptorSet::AllocateDescriptorSet() {
    if (mSet != VK_NULL_HANDLE) {
        CHECK_VK_RESULT(vkFreeDescriptorSets(m_device->Get(), mDescriptorPool, 1, &mSet));
    }
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = mDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &mSetLayout;
    CHECK_VK_RESULT(vkAllocateDescriptorSets(m_device->Get(), &allocInfo, &mSet));
    if (!mSet) {
        LOG_ERROR("failed to allocate descriptorset");
    }
}

void DescriptorSet::CreateDescriptorPool() {

    std::unordered_map<VkDescriptorType, u32> descriptorTypeMap;

    for (u32 binding = 0; binding < mDescriptorSetInfo->bindingCount; binding++) {
        descriptorTypeMap[ToVkDescriptorType(mDescriptorSetInfo->types[binding])]++;
    }

    std::vector<VkDescriptorPoolSize> poolSizes(descriptorTypeMap.size());

    u32 i = 0;
    for (auto &type : descriptorTypeMap) {
        poolSizes[i++] = VkDescriptorPoolSize{type.first, type.second};
    }

    VkDescriptorPoolCreateInfo poolInfo{};

    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.pNext = nullptr;
    poolInfo.flags = 0;
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = static_cast<u32>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    CHECK_VK_RESULT(vkCreateDescriptorPool(m_device->Get(), &poolInfo, nullptr, &mDescriptorPool));
}

void DescriptorSetInfo::AddBinding(DescriptorType type, u32 stage) {
    bindingCount++;
    types.push_back(type);
    stageFlags.push_back(stage);
}

void DescriptorSetUpdateDesc::BindResource(u32 binding, std::shared_ptr<DescriptorBase> buffer) {
    descriptorMap[binding] = buffer;
}

} // namespace Horizon