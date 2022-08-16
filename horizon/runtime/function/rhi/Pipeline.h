#pragma once

#include <unordered_map>
#include <vector>

#include <runtime/function/rhi/RHIUtils.h>
#include <runtime/function/rhi/ShaderProgram.h>

namespace Horizon::RHI {
// struct viewport create info

class Pipeline {
  public:
    Pipeline() noexcept;
    ~Pipeline() noexcept;

    PipelineType GetType() const noexcept;

    virtual void SetComputeShader(ShaderProgram *vs) noexcept = 0;
    virtual void SetGraphicsShader(ShaderProgram *vs,
                                   ShaderProgram *ps) noexcept = 0;
    //// vertex input state
    // virtual void SetVertexInputState(const VertexInputStateCreateInfo&
    // vertex_input_state_create_info) noexcept = 0;
    //// input assembly state
    // virtual void SetInputAssemblyState(const InputAssemblyStateCreateInfo&
    // input_assembly_state_create_info) noexcept = 0;
    //// viewport state
    // virtual void SetViewportState(const ViewportStateCreateInfo&
    // viewport_state_create_info) noexcept = 0;
    //// rasterization state
    // virtual void SetRasterizationState(const RasterizationStateCreateInfo&
    // rasterization_state_create_info) noexcept = 0;
    //// multisample state
    // virtual void SetMultisampleState(const MultisampleStateCreateInfo&
    // multisample_state_create_info) noexcept = 0;
    //// depth stencil state
    // virtual void SetDepthStencilState(const DepthStencilStateCreateInfo&
    // depth_stencil_state_create_info) noexcept = 0;
    //// color blend state
    // virtual void SetColorBlendState(const ColorBlendStateCreateInfo&
    // color_blend_state_create_info) noexcept = 0;
    //// dynamic state
    // virtual void SetDynamicState(const DynamicStateCreateInfo&
    // dynamic_state_create_info) noexcept = 0;
    //// pipeline layout
    // virtual void SetPipelineLayout(const PipelineLayoutCreateInfo&
    // pipeline_layout_create_info) noexcept = 0;
    //// render pass
    // virtual void SetRenderPass(const RenderPassCreateInfo&
    // render_pass_create_info) noexcept = 0;

  protected:
    std::unordered_map<ShaderType, ShaderProgram *> shader_map{};
    PipelineCreateInfo m_create_info{};
};
} // namespace Horizon::RHI
