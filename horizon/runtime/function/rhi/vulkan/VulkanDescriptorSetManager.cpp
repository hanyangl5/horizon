#include <algorithm>
#include <runtime/function/rhi/vulkan/VulkanDescriptorSetManager.h>
#include <runtime/function/rhi/vulkan/VulkanShaderProgram.h>

namespace Horizon::RHI {

VulkanDescriptorSetManager::VulkanDescriptorSetManager(
    const VulkanRendererContext &context) noexcept
    : m_context(context) {}

std::vector<u32> VulkanDescriptorSetManager::CreateLayouts(
    std::unordered_map<ShaderType, ShaderProgram *> &shader_map,
    PipelineType pipeline_type) noexcept {

    std::vector<u32> pipeline_layout;
    
    u32 set_count{};
    std::vector<VkDescriptorSetLayout> set_layouts{};
    std::vector<u32> set_numbers{};
    VkDescriptorSetLayoutCreateInfo set_layout_create_info{};
    set_layout_create_info.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;

    // combine vs/gs/ps to get pipeline layout
    if (pipeline_type == PipelineType::GRAPHICS) {
        auto vk_vs = static_cast<VulkanShaderProgram *>(
            shader_map[ShaderType::VERTEX_SHADER]);
        auto vk_ps = static_cast<VulkanShaderProgram *>(
            shader_map[ShaderType::PIXEL_SHADER]);

        auto &vs_sets = vk_vs->sets;
        auto &ps_sets = vk_ps->sets;

        u32 set_count =
            vs_sets.size() >= ps_sets.size() ? vs_sets.size() : ps_sets.size();

    } else if (pipeline_type == PipelineType::COMPUTE) {
        auto vk_cs = static_cast<VulkanShaderProgram *>(
            shader_map[ShaderType::COMPUTE_SHADER]);
        auto &cs_sets = vk_cs->sets;

        set_count = cs_sets.size();
        set_layouts.resize(set_count);
        set_numbers.resize(set_count);

        for (u32 i = 0; i < set_count; i++) {
            const auto &refl_set = *(cs_sets[i]);

            set_layout_create_info.bindingCount = refl_set.binding_count;
            std::vector<VkDescriptorSetLayoutBinding> layout_bindings(
                refl_set.binding_count);

            for (u32 binding = 0; binding < refl_set.binding_count; binding++) {
                const SpvReflectDescriptorBinding &refl_binding =
                    *(refl_set.bindings[binding]);
                layout_bindings[binding].binding = refl_binding.binding;
                layout_bindings[binding].descriptorType =
                    static_cast<VkDescriptorType>(refl_binding.descriptor_type);
                layout_bindings[binding].descriptorCount = 1;

                for (u32 i_dim = 0; i_dim < refl_binding.array.dims_count;
                     ++i_dim) {
                    layout_bindings[binding].descriptorCount *=
                        refl_binding.array.dims[i_dim];
                }
                layout_bindings[binding].stageFlags =
                    VK_SHADER_STAGE_VERTEX_BIT;
            }
            set_layout_create_info.bindingCount = layout_bindings.size();
            set_layout_create_info.pBindings = layout_bindings.data();
            set_numbers[i] = refl_set.set;
            u64 hash_value = std::hash<VkDescriptorSetLayoutCreateInfo>{}(
                set_layout_create_info);
            pipeline_layout.emplace_back(hash_value);
            if (!descriptor_set_map.at(hash_value)) {
                vkCreateDescriptorSetLayout(m_context.device,
                                            &set_layout_create_info, nullptr,
                                            &set_layouts[i]);

                std::pair<u64, VkDescriptorSet> p{hash_value,
                                                  VkDescriptorSet{}};
                descriptor_set_map.emplace(p);
            }

        }

    } else if (pipeline_type == PipelineType::RAY_TRACING) {
        // TODO
    }

    return std::vector<u32>();
}

VulkanDescriptorSetManager::~VulkanDescriptorSetManager() {
    // TODO: destroy descriptor resources, descriptorsetlayout, descriptorpool
}
// TODO: reallocate global descriptorset per frame?
void VulkanDescriptorSetManager::AllocateDescriptors() noexcept {

}
void VulkanDescriptorSetManager::ResetDescriptorPool() noexcept {
    //vkResetDescriptorPool(m_device, m_bindless_descriptor_pool, 0);
}
void VulkanDescriptorSetManager::Update() noexcept {

    // TODO: create empty descriptors if no resource set

    //vkUpdateDescriptorSets(m_device, descriptor_writes.size(),
    //                       descriptor_writes.data(), 0, nullptr);
}
} // namespace Horizon::RHI