#pragma once

// standard libraries
#include <thread>
#include <utility>

// third party libraries

// project headers
#include <runtime/core/window/window.h>
#include <runtime/function/rhi/buffer.h>
#include <runtime/function/rhi/command_context.h>
#include <runtime/function/rhi/command_list.h>
#include <runtime/function/rhi/rhi_utils.h>
#include <runtime/function/rhi/render_target.h>
#include <runtime/function/rhi/sampler.h>
#include <runtime/function/rhi/semaphore.h>
#include <runtime/function/rhi/swap_chain.h>
#include <runtime/function/rhi/texture.h>

namespace Horizon::Backend {

extern thread_local CommandContext *thread_command_context;

struct QueueSubmitInfo {
    CommandQueueType queue_type;
    Container::Array<CommandList *> command_lists;
    Container::Array<Semaphore *> wait_semaphores;
    Container::Array<Semaphore *> signal_semaphores;
    bool wait_image_acquired = false;
    bool signal_render_complete = false;
};

struct QueuePresentInfo {
    // Container::Array<Semaphore *> wait_semaphores; // we only need to wait render complete semaphore
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

    virtual Buffer *CreateBuffer(const BufferCreateInfo &buffer_create_info) = 0;

    virtual void DestroyBuffer(Buffer *buffer) = 0;

    virtual Texture *CreateTexture(const TextureCreateInfo &texture_create_info) = 0;

    virtual void DestroyTexture(Texture *texture) = 0;

    virtual RenderTarget *CreateRenderTarget(const RenderTargetCreateInfo &render_target_create_info) = 0;

    virtual void DestroyRenderTarget(RenderTarget *render_target) = 0;

    virtual SwapChain *CreateSwapChain(const SwapChainCreateInfo &create_info) = 0;

    virtual void DestroySwapChain(SwapChain *swap_chain) = 0;

    // TODO: modify this with new shader system
    virtual Shader *CreateShader(ShaderType type, const std::filesystem::path &file_name) = 0;

    virtual void DestroyShader(Shader *shader_program) = 0;

    virtual Pipeline *CreateGraphicsPipeline(const GraphicsPipelineCreateInfo &create_info) = 0;

    virtual Pipeline *CreateComputePipeline(const ComputePipelineCreateInfo &create_info) = 0;

    virtual void DestroyPipeline(Pipeline *pipeline) = 0;

    virtual CommandList *GetCommandList(CommandQueueType type) = 0;

    virtual Semaphore *CreateSemaphore1() = 0;

    virtual void DestroySemaphore(Semaphore *semaphore) = 0;

    virtual Sampler *CreateSampler(const SamplerDesc &sampler_desc) = 0;

    virtual void DestroySampler(Sampler *sampler) = 0;

    virtual void WaitGpuExecution(CommandQueueType queue_type) = 0;

    virtual void ResetRHIResources() = 0;

    virtual void ResetFence(CommandQueueType queue_type) = 0;

    // submit command list to command queue
    virtual void SubmitCommandLists(const QueueSubmitInfo &queue_submit_info) = 0;

    virtual void AcquireNextFrame(SwapChain *swap_chain) = 0;

    virtual void Present(const QueuePresentInfo &queue_present_info) = 0;

    void SetWindow(Window *window) noexcept { m_window = window; }

    //virtual void SetDebugName(void *resource, const Container::String &name) = 0;

  protected:
    u32 m_current_frame_index{0};
    Window *m_window{};
    bool m_offscreen{false};
    bool gpu_support_async_compute{false};
    bool gpu_support_async_transfer{false};
};

} // namespace Horizon::Backend

namespace Horizon {
using Buffer = Backend::Buffer;
using Texture = Backend::Texture;
using RenderTarget = Backend::RenderTarget;
using Shader = Backend::Shader;
using Pipeline = Backend::Pipeline;
using CommandList = Backend::CommandList;
} // namespace Horizon