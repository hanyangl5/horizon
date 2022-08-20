#pragma once

#include <memory>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include <runtime/core/window/Window.h>
#include <runtime/function/rhi/Buffer.h>
#include <runtime/function/rhi/CommandContext.h>
#include <runtime/function/rhi/CommandList.h>
#include <runtime/function/rhi/RHIUtils.h>
#include <runtime/function/rhi/Texture.h>
#include <runtime/function/shader_compiler/ShaderCompiler.h>

namespace Horizon::RHI {

extern thread_local std::unique_ptr<CommandContext> thread_command_context;

class RHI {
  public:
    RHI() noexcept;

    virtual ~RHI() noexcept;

    RHI(const RHI &rhi) noexcept = delete;

    RHI &operator=(const RHI &rhi) noexcept = delete;

    RHI(RHI &&rhi) noexcept = delete;

    RHI &operator=(RHI &&rhi) noexcept = delete;

    virtual void InitializeRenderer() = 0;

    virtual Resource<Buffer> CreateBuffer(const BufferCreateInfo &buffer_create_info) = 0;

    virtual Resource<Texture> CreateTexture(const TextureCreateInfo &texture_create_info) = 0;

    virtual void CreateSwapChain(Window *window) = 0;

    virtual ShaderProgram *CreateShaderProgram(ShaderType type, const std::string &entry_point, u32 compile_flags,
                                               std::string file_name) = 0;
    virtual void DestroyShaderProgram(ShaderProgram *shader_program) = 0;
    // virtual void CreateRenderTarget() = 0;

    virtual Pipeline *CreateGraphicsPipeline(const GraphicsPipelineCreateInfo &create_info) = 0;

    virtual Pipeline *CreateComputePipeline(const ComputePipelineCreateInfo &create_info) = 0;

    virtual CommandList *GetCommandList(CommandQueueType type) = 0;

    virtual void WaitGpuExecution(CommandQueueType queue_type) = 0;

    virtual void ResetCommandResources() = 0;

    // submit command list to command queue
    virtual void SubmitCommandLists(CommandQueueType queue, std::vector<CommandList *> &command_lists) = 0;

    virtual void SetResource(Buffer *buffer, Pipeline *pipeline, u32 set, u32 binding) = 0;

    virtual void SetResource(Texture *texture) = 0;

    virtual void UpdateDescriptors() = 0;

  protected:
    u32 m_back_buffer_count{2};
    u32 m_current_frame_index{0};
    std::shared_ptr<ShaderCompiler> m_shader_compiler{};

    // std::unordered_map<u64, Pipeline *> pipeline_map; // TODO: manage by rg

  private:
    std::shared_ptr<Window> m_window{};
};

} // namespace Horizon::RHI