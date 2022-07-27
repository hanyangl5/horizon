#include <runtime/function/rhi/vulkan/VulkanPipeline.h>
#include <runtime/function/rhi/vulkan/VulkanShaderProgram.h>

namespace Horizon::RHI {
VulkanPipeline::VulkanPipeline(VkDevice device,
                               const PipelineCreateInfo &pipeline_create_info,
                               VulkanDescriptor *descriptor) noexcept
    : m_device(device), m_descriptor(descriptor) {
    m_type = pipeline_create_info.type;
}

VulkanPipeline::~VulkanPipeline() noexcept {
    // TODO: destroy pipeline resources, pipeline layout, pipeline
}

void VulkanPipeline::Create() noexcept {

    // at that time parse shader and generate descriptor layout and pipeline
    // layout.
    CreatePipelineLayout();

    switch (m_type) {
    case PipelineType::COMPUTE:
        CreateComputePipeline();
        break;
    case PipelineType::GRAPHICS:
        CreateGraphicsPipeline();
        break;
    case PipelineType::RAY_TRACING:
        // VkRayTracingPipelineCreateInfoKHR r	t_pipeline_create_info{};
        // vkCreateRayTracingPipelinesKHR();
        break;
    default:
        break;
    }
}

void VulkanPipeline::SetShader(ShaderProgram *shader_moudle) noexcept {
    switch (shader_moudle->GetType()) {
    case ShaderType::VERTEX_SHADER:
    // case ShaderType::GEOMETRY_SHADER:
    case ShaderType::PIXEL_SHADER:
        assert(m_type == PipelineType::GRAPHICS);
        // return;
    case ShaderType::COMPUTE_SHADER:
        assert(m_type == PipelineType::COMPUTE);
        // return;
    }

    shader_map[shader_moudle->GetType()] = shader_moudle;
}

void VulkanPipeline::CreateGraphicsPipeline() noexcept {
    VkGraphicsPipelineCreateInfo graphics_pipeline_create_info{};

    CHECK_VK_RESULT(vkCreateGraphicsPipelines(m_device, nullptr, 1,
                                              &graphics_pipeline_create_info,
                                              nullptr, &m_pipeline));
}
void VulkanPipeline::CreateComputePipeline() noexcept {
    if (!shader_map[ShaderType::COMPUTE_SHADER]) {
        LOG_ERROR("missing shader: compute shader");
        return;
    }
    auto cs = static_cast<VulkanShaderProgram *>(
        shader_map[ShaderType::COMPUTE_SHADER]);
    VkPipelineShaderStageCreateInfo shader_stage_create_info{};
    shader_stage_create_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stage_create_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    shader_stage_create_info.module = cs->m_shader_module;
    shader_stage_create_info.pName = cs->GetEntryPoint().c_str();

    VkComputePipelineCreateInfo compute_pipeline_create_info{};

    compute_pipeline_create_info.sType =
        VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    compute_pipeline_create_info.flags = 0;
    compute_pipeline_create_info.layout = m_pipeline_layout;
    compute_pipeline_create_info.stage = shader_stage_create_info;
    compute_pipeline_create_info.basePipelineHandle = nullptr;
    compute_pipeline_create_info.basePipelineIndex = 0;
    CHECK_VK_RESULT(vkCreateComputePipelines(m_device, nullptr, 1,
                                             &compute_pipeline_create_info,
                                             nullptr, &m_pipeline));
}
void VulkanPipeline::CreatePipelineLayout() noexcept {

    u32 set_count{};
    std::vector<VkDescriptorSetLayout> set_layouts{};
    std::vector<u32> set_numbers{};
    VkDescriptorSetLayoutCreateInfo set_layout_create_info{};
    set_layout_create_info.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;

    // for graphics pipeline, descriptorset reflected in vs/gs/ts/ps conbine to the final descriptor/pipeline layout
    if (m_type == PipelineType::GRAPHICS) {
        auto vk_vs = static_cast<VulkanShaderProgram *>(
            shader_map[ShaderType::VERTEX_SHADER]);
        auto vk_ps = static_cast<VulkanShaderProgram *>(
            shader_map[ShaderType::PIXEL_SHADER]);

        auto &vs_sets = vk_vs->sets;
        auto &ps_sets = vk_ps->sets;

        u32 set_count =
            vs_sets.size() >= ps_sets.size() ? vs_sets.size() : ps_sets.size();

    } else if (m_type == PipelineType::COMPUTE) {
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
            vkCreateDescriptorSetLayout(m_device, &set_layout_create_info,
                                        nullptr, &set_layouts[i]);
        }

    } else if (m_type == PipelineType::RAY_TRACING) {
        // TODO
    }

    VkPipelineLayoutCreateInfo pipeline_layout_create_info{};

    pipeline_layout_create_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_create_info.setLayoutCount =
        static_cast<u32>(set_layouts.size());
    pipeline_layout_create_info.pSetLayouts = set_layouts.data();
    CHECK_VK_RESULT(vkCreatePipelineLayout(
        m_device, &pipeline_layout_create_info, nullptr, &m_pipeline_layout));
}
} // namespace Horizon::RHI