#pragma once

#include <vulkan/vulkan.h>

#include <runtime/core/window/Window.h>
#include <runtime/function/rhi/RHIUtils.h>

#include <runtime/function/rhi/RHI.h>
#include <runtime/function/scene/camera/Camera.h>

namespace Horizon {

using Buffer = RHI::Buffer;
using Texture = RHI::Texture;
using ShaderProgram = RHI::ShaderProgram;
using Pipeline = RHI::Pipeline;
using CommandList = RHI::CommandList;

class RenderSystem {
  public:
    RenderSystem(u32 width, u32 height, Window *window,
                 RenderBackend backend) noexcept;

    ~RenderSystem() noexcept;

    RenderSystem(const RenderSystem &rhs) noexcept = delete;

    RenderSystem &operator=(const RenderSystem &rhs) noexcept = delete;

    RenderSystem(RenderSystem &&rhs) noexcept = delete;

    RenderSystem &operator=(RenderSystem &&rhs) noexcept = delete;

  public:
    Camera *GetMainCamera() const noexcept;

    // Resource<Buffer>
    // CreateBuffer(const BufferCreateInfo &buffer_create_info) noexcept;

    // Resource<Texture>
    // CreateTexture(const TextureCreateInfo &texture_create_info) noexcept;

    // ShaderProgram *CreateShaderProgram(ShaderType type,
    //                                    const std::string &entry_point,
    //                                    u32 compile_flags,
    //                                    std::string file_name) noexcept;
    // void DestroyShaderProgram(ShaderProgram *shader_program) noexcept;

    // Pipeline *CreateGraphicsPipeline(
    //     const GraphicsPipelineCreateInfo &create_info) noexcept;

    // Pipeline *CreateComputePipeline(
    //     const ComputePipelineCreateInfo &create_info) noexcept;

    // CommandList *GetCommandList(CommandQueueType type) noexcept;

    // void WaitGpuExecution(CommandQueueType queue_type) noexcept;

    // void ResetCommandResources() noexcept;

    // // submit command list to command queue
    // void SubmitCommandLists(CommandQueueType queue,
    //                         std::vector<CommandList *> &command_lists)
    //                         noexcept;

    // void SetResource(Buffer *buffer, Pipeline *pipeline, u32 set,
    //                  u32 binding) noexcept;

    // void SetResource(Texture *texture) noexcept;

    // void UpdateDescriptors() noexcept;

    RHI::RHI *GetRhi() noexcept { return m_rhi.get(); }

  private:
    void InitializeRenderAPI(RenderBackend backend) noexcept;

  private:
    Window *m_window{};

    std::unique_ptr<RHI::RHI> m_rhi{};
};
} // namespace Horizon