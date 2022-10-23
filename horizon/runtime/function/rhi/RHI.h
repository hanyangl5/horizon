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
#include <runtime/function/rhi/RenderTarget.h>
#include <runtime/function/rhi/Semaphore.h>
#include <runtime/function/rhi/Texture.h>
#include <runtime/function/rhi/Sampler.h>
#include <runtime/function/rhi/SwapChain.h>

namespace Horizon::Backend {

extern thread_local std::unique_ptr<CommandContext> thread_command_context;

struct QueueSubmitInfo {
    CommandQueueType queue_type;
    std::vector<CommandList *> command_lists;
    std::vector<Semaphore *> wait_semaphores;
    std::vector<Semaphore *> signal_semaphores;
    bool wait_image_acquired = false;
    bool signal_render_complete = false;
};

struct QueuePresentInfo {
    //std::vector<Semaphore *> wait_semaphores; // we only need to wait render complete semaphore
    SwapChain *swap_chain;
};

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

    virtual Resource<RenderTarget> CreateRenderTarget(const RenderTargetCreateInfo &render_target_create_info) = 0;

    virtual Resource<SwapChain> CreateSwapChain(const SwapChainCreateInfo &create_info) = 0;

    virtual Shader *CreateShader(ShaderType type, u32 compile_flags, const std::filesystem::path& file_name) = 0;
    
    virtual void DestroyShader(Shader *shader_program) = 0;

    virtual Pipeline *CreateGraphicsPipeline(const GraphicsPipelineCreateInfo &create_info) = 0;

    virtual Pipeline *CreateComputePipeline(const ComputePipelineCreateInfo &create_info) = 0;

    virtual void DestroyPipeline(Pipeline *pipeline) = 0;

    virtual CommandList *GetCommandList(CommandQueueType type) = 0;

    virtual Resource<Semaphore> GetSemaphore() = 0;

    virtual Resource<Sampler> GetSampler(const SamplerDesc &sampler_desc) = 0;

    virtual void WaitGpuExecution(CommandQueueType queue_type) = 0;

    virtual void ResetRHIResources() = 0;

    virtual void ResetFence(CommandQueueType queue_type) = 0;

    // submit command list to command queue
    virtual void SubmitCommandLists(const QueueSubmitInfo& queue_submit_info) = 0;

    virtual void AcquireNextFrame(SwapChain* swap_chain) = 0;

    virtual void Present(const QueuePresentInfo& queue_present_info) = 0;

    void SetWindow(Window *window) noexcept { m_window = window; }

  protected:
    u32 m_current_frame_index{0};
    Window *m_window{};
    bool m_offscreen{false};
};

} // namespace Horizon::Backend