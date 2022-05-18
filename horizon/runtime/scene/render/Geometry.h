#pragma once
#include <memory>
#include <runtime/function/rhi/vulkan/Descriptors.h>
#include <runtime/function/rhi/vulkan/Pipeline.h>
#include <runtime/scene/scene/Scene.h>

namespace Horizon
{

    class Geometry
    {
    public:
        Geometry(std::shared_ptr<Scene> _scene, std::shared_ptr<PipelineManager> _pipeline_manager, std::shared_ptr<Device> _device, RenderContext &_render_context) noexcept;
        ~Geometry() noexcept;
        // void UpdateDescriptorSets() noexcept;
        void BindResource(u32 binding, std::shared_ptr<DescriptorBase> buffer) noexcept;
        std::shared_ptr<AttachmentDescriptor> GetFrameBufferAttachment(u32 _index) const noexcept;
        std::shared_ptr<Pipeline> GetPipeline() const noexcept;

    private:
        std::shared_ptr<Pipeline> m_pipeline;
    };

}
