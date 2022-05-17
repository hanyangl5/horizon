#include <runtime/function/render/rhi/Descriptors.h>
#include <runtime/function/render/rhi/Pipeline.h>
#include <memory>

namespace Horizon
{
    class PostProcess
    {
    public:
        PostProcess(std::shared_ptr<PipelineManager> _pipeline_manager, std::shared_ptr<Device> _device, RenderContext &_render_context) noexcept;
        ~PostProcess() noexcept;
        void UpdateDescriptorSets() noexcept;
        void BindResource(u32 binding, std::shared_ptr<DescriptorBase> buffer) noexcept;
        std::shared_ptr<AttachmentDescriptor> GetFrameBufferAttachment(u32 _index) const noexcept;
        std::shared_ptr<DescriptorSet> GetDescriptorSet() const noexcept;
        std::shared_ptr<Pipeline> GetPipeline() const noexcept;
    private:
        void CreateResources() noexcept;
    private:
        std::shared_ptr<Pipeline> m_pipeline;
        //std::shared_ptr<Pipeline> m_tone_mapping_pass;

        //std::shared_ptr<DescriptorSetLayouts> tone_mapping_descriptor_set_layouts;

        std::shared_ptr<DescriptorSet> m_pp_descriptorset;
        //std::shared_ptr<DescriptorSet> m_tone_mapping_descriptor_set;

        //DescriptorSetUpdateDesc m_tone_mapping_descriptor_set_update_desc;
        DescriptorSetUpdateDesc m_descriptor_set_update_desc;
    };

}
