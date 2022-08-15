#pragma once

#include <memory>
#include <thread>
#include <unordered_map>
#include <vector>
#include <utility>

#include <runtime/core/window/Window.h>
#include <runtime/function/rhi/Buffer.h>
#include <runtime/function/rhi/CommandList.h>
#include <runtime/function/rhi/CommandContext.h>
#include <runtime/function/rhi/RHIUtils.h>
#include <runtime/function/rhi/Texture.h>
#include <runtime/function/shader_compiler/ShaderCompiler.h>

namespace Horizon::RHI {

extern thread_local std::unique_ptr<CommandContext> thread_command_context;

class RHIInterface {
  public:
    RHIInterface() noexcept;

    virtual ~RHIInterface() noexcept;

    RHIInterface(const RHIInterface &window) noexcept = delete;

    RHIInterface &operator=(const RHIInterface &window) noexcept = delete;

    RHIInterface(RHIInterface &&window) noexcept = delete;

    RHIInterface &operator=(RHIInterface &&window) noexcept = delete;

    virtual void InitializeRenderer() noexcept = 0;

    virtual Resource<Buffer>
    CreateBuffer(const BufferCreateInfo &buffer_create_info) noexcept = 0;

    virtual Resource<Texture>
    CreateTexture(const TextureCreateInfo &texture_create_info) noexcept = 0;

    virtual void CreateSwapChain(Window *window) noexcept = 0;

    virtual ShaderProgram *
    CreateShaderProgram(ShaderType type, const std::string &entry_point,
                        u32 compile_flags, std::string file_name) noexcept = 0;
    virtual void
    DestroyShaderProgram(ShaderProgram *shader_program) noexcept = 0;
    // virtual void CreateRenderTarget() = 0;
    virtual Pipeline *
    CreatePipeline(const PipelineCreateInfo &pipeline_create_info) noexcept = 0;
    // virtual void CreateDescriptorSet() = 0;

    virtual CommandList *GetCommandList(CommandQueueType type) noexcept = 0;

    virtual void WaitGpuExecution(CommandQueueType queue_type) noexcept = 0;

    virtual void ResetCommandResources() noexcept = 0;

    // submit command list to command queue
    virtual void
    SubmitCommandLists(CommandQueueType queue,
                       std::vector<CommandList *> &command_lists) noexcept = 0;

    virtual void SetResource(Buffer *buffer, Pipeline *pipeline, u32 set,
                             u32 binding) noexcept = 0;
    virtual void SetResource(Texture *texture) noexcept = 0;

    virtual void UpdateDescriptors() noexcept = 0;

  protected:
    u32 m_back_buffer_count{2};
    u32 m_current_frame_index{0};
    std::shared_ptr<ShaderCompiler> m_shader_compiler{};
    // each thread has one command pool,
    //std::unordered_map<std::thread::id, std::unique_ptr<CommandContext>>
    //    m_command_context_map{};
    std::mutex m_command_context_mutex;

  private:
    std::shared_ptr<Window> m_window{};
};

} // namespace Horizon::RHI