#include "Atmosphere.h"
#include <runtime/function/render/rhi/VulkanEnums.h>
#include <runtime/function/render/RenderContext.h>
#include <runtime/core/path/Path.h>

namespace Horizon
{
    Atmosphere::Atmosphere(std::shared_ptr<PipelineManager> _pipeline_manager, std::shared_ptr<Device> _device, RenderContext &_render_context) noexcept
    {

        // scattering pass

        std::shared_ptr<DescriptorSetInfo> scatter_descriptor_set_create_info = std::make_shared<DescriptorSetInfo>();
        scatter_descriptor_set_create_info->AddBinding(DESCRIPTOR_TYPE_UNIFORM_BUFFER, SHADER_STAGE_PIXEL_SHADER); // camera params
        scatter_descriptor_set_create_info->AddBinding(DESCRIPTOR_TYPE_UNIFORM_BUFFER, SHADER_STAGE_PIXEL_SHADER); // scatter params

        m_scatter_descriptorset = std::make_shared<DescriptorSet>(_device, scatter_descriptor_set_create_info);

        std::shared_ptr<DescriptorSetLayouts> scatter_descriptor_set_layout = std::make_shared<DescriptorSetLayouts>();
        // scatter_descriptor_set_layout = m_scene->GetGeometryPassDescriptorLayouts();
        scatter_descriptor_set_layout->layouts.push_back(m_scatter_descriptorset->GetLayout());

        PipelineCreateInfo scatter_pipeline_create_info;
        scatter_pipeline_create_info.name = "scatter";
        scatter_pipeline_create_info.vs = std::make_shared<Shader>(_device->Get(), Path::GetInstance().GetShaderPath("scatter.vert.spv"));
        scatter_pipeline_create_info.ps = std::make_shared<Shader>(_device->Get(), Path::GetInstance().GetShaderPath("scatter.frag.spv"));
        scatter_pipeline_create_info.descriptor_layouts = scatter_descriptor_set_layout;
        scatter_pipeline_create_info.pipeline_descriptor_set = m_scatter_descriptorset;
        std::vector<AttachmentCreateInfo> scatter_attachments_create_info{
            {VK_FORMAT_R8G8B8A8_UNORM, COLOR_ATTACHMENT, _render_context.width, _render_context.height}};

        _pipeline_manager->createPipeline(scatter_pipeline_create_info, scatter_attachments_create_info, _render_context);

        m_pipeline = _pipeline_manager->Get("scatter");
        m_scattering_ub = std::make_shared<UniformBuffer>(_device);
        m_scattering_ubdata.resolution = Math::vec2(_render_context.width, _render_context.height);
    }

    Atmosphere::~Atmosphere() noexcept
    {
    }

    void Atmosphere::SetInvViewProjectionMatrix(Math::mat4 _matrix) noexcept
    {
        m_scattering_ubdata.inv_view_projection_matrix = _matrix;
        m_scattering_ub->update(&m_scattering_ubdata, sizeof(ScatteringUb));
    }

    void Atmosphere::UpdateDescriptorSets() noexcept
    {

        m_descriptor_set_update_desc.BindResource(1, m_scattering_ub);

        m_scatter_descriptorset->AllocateDescriptorSet();
        m_scatter_descriptorset->UpdateDescriptorSet(m_descriptor_set_update_desc);
    }

    void Atmosphere::BindResource(u32 binding, std::shared_ptr<DescriptorBase> buffer) noexcept
    {
        m_descriptor_set_update_desc.BindResource(binding, buffer);
    }

    std::shared_ptr<AttachmentDescriptor> Atmosphere::GetFrameBufferAttachment(u32 _index) const noexcept
    {
        return m_pipeline->GetFrameBufferAttachment(_index);
    }

    std::shared_ptr<DescriptorSet> Atmosphere::GetDescriptorSet() const noexcept
    {
        return m_scatter_descriptorset;
    }

    std::shared_ptr<Pipeline> Atmosphere::GetPipeline() const noexcept
    {
        return m_pipeline;
    }

}