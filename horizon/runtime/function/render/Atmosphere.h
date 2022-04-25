#pragma once
#include <runtime/function/render/rhi/Descriptors.h>
#include <runtime/function/render/rhi/Pipeline.h>
#include <memory>
#include <runtime/function/render/rhi/UniformBuffer.h>
namespace Horizon
{
    class Atmosphere
    {
    public:
        Atmosphere(std::shared_ptr<PipelineManager> _pipeline_manager, std::shared_ptr<Device> _device, RenderContext &_render_context) noexcept;
        ~Atmosphere() noexcept;
        void SetInvViewProjectionMatrix(Math::mat4 _matrix) noexcept;
        void UpdateDescriptorSets() noexcept;
        void BindResource(u32 binding, std::shared_ptr<DescriptorBase> buffer) noexcept;
        std::shared_ptr<AttachmentDescriptor> GetFrameBufferAttachment(u32 _index) const noexcept;
        std::shared_ptr<DescriptorSet> GetDescriptorSet() const noexcept;
        std::shared_ptr<Pipeline> GetPipeline() const noexcept;

    private:
        std::shared_ptr<Pipeline> m_pipeline;
        std::shared_ptr<DescriptorSet> m_scatter_descriptorset;

        DescriptorSetUpdateDesc m_descriptor_set_update_desc;

        std::shared_ptr<UniformBuffer> m_scattering_ub;
        struct ScatteringUb
        {
            Math::mat4 inv_view_projection_matrix;
            Math::vec2 resolution;
        } m_scattering_ubdata;
    };

}
