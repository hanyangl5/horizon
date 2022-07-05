#pragma once

#include <vulkan/vulkan.h>

#include <runtime/core/window/Window.h>
#include <runtime/function/rhi/RHIUtils.h>

#include <runtime/function/rhi/RHIInterface.h>
#include <runtime/function/scene/camera/Camera.h>

namespace Horizon {

using Buffer = RHI::Buffer;
using Texture = RHI::Texture;
using ShaderProgram = RHI::ShaderProgram;
using Pipeline = RHI::Pipeline;
using CommandList = RHI::CommandList;

class RenderSystem {
  public:
  public:
    RenderSystem(u32 width, u32 height, Window *window,
                 RenderBackend backend) noexcept;

    ~RenderSystem() noexcept;

    Camera *GetMainCamera() const noexcept;

    Resource<Buffer>
    CreateBuffer(const BufferCreateInfo &buffer_create_info) noexcept;

    Resource<Texture>
    CreateTexture(const TextureCreateInfo &texture_create_info) noexcept;

    ShaderProgram *CreateShaderProgram(ShaderType type,
                                       const std::string &entry_point,
                                       u32 compile_flags,
                                       std::string file_name) noexcept;
    void DestroyShaderProgram(ShaderProgram *shader_program) noexcept;
    Pipeline *
    CreatePipeline(const PipelineCreateInfo &pipeline_create_info) noexcept;

    CommandList *GetCommandList(CommandQueueType type) noexcept;
    void ResetCommandResources() noexcept;

    // submit command list to command queue
    void SubmitCommandLists(CommandQueueType queue,
                            std::vector<CommandList *> &command_lists) noexcept;
    void SetResource(Buffer *buffer) noexcept;
    void SetResource(Texture *texture) noexcept;

    void UpdateDescriptors() noexcept;
  private:
    void InitializeRenderAPI(RenderBackend backend) noexcept;

  private:
    Window *m_window{};

    std::unique_ptr<RHI::RHIInterface> m_render_api{};
};
} // namespace Horizon