#pragma once
#include <memory>
#include <runtime/function/rhi/vulkan/Descriptors.h>
#include <runtime/function/rhi/vulkan/Pipeline.h>
#include <runtime/scene/scene/Scene.h>

namespace Horizon {

class LightPass {
  public:
    LightPass(std::shared_ptr<Scene> _scene, std::shared_ptr<PipelineManager> _pipeline_manager,
              std::shared_ptr<Device> _device, RenderContext &_render_context) noexcept;
    ~LightPass() noexcept;
    void CreateResources() noexcept;
    void UpdateDescriptorSets() noexcept;
    void BindResource(u32 binding, std::shared_ptr<DescriptorBase> buffer) noexcept;
    std::shared_ptr<AttachmentDescriptor> GetFrameBufferAttachment(u32 _index) const noexcept;
    std::shared_ptr<Pipeline> GetPipeline() const noexcept;
    std::shared_ptr<DescriptorSet> m_descriptorset;

  private:
    std::shared_ptr<Device> m_device;
    std::shared_ptr<Pipeline> m_pipeline;

    std::shared_ptr<DescriptorSetLayouts> m_descriptor_set_layout;
    DescriptorSetUpdateDesc m_descriptor_set_update_desc;
};

} // namespace Horizon
