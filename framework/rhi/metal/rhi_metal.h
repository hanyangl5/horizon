#pragma once

#include <rhi/metal/metal_utils.h>
#include <rhi/rhi.h>

namespace Horizon::Backend
{

class MetalCommandList;

class MetalRHI final : public RHI
{
  public:
    explicit MetalRHI(bool offscreen) noexcept;
    ~MetalRHI() noexcept override;
    MetalRHI(const MetalRHI &rhs) noexcept = delete;
    MetalRHI &operator=(const MetalRHI &rhs) noexcept = delete;
    MetalRHI(MetalRHI &&rhs) noexcept = delete;
    MetalRHI &operator=(MetalRHI &&rhs) noexcept = delete;

    void InitializeRenderer() override;

    Buffer *CreateBuffer(const BufferCreateInfo &buffer_create_info) override;
    void DestroyBuffer(Buffer *buffer) override;

    Texture *CreateTexture(const TextureCreateInfo &texture_create_info) override;
    void DestroyTexture(Texture *texture) override;

    RenderTarget *CreateRenderTarget(const RenderTargetCreateInfo &render_target_create_info) override;
    void DestroyRenderTarget(RenderTarget *render_target) override;

    SwapChain *CreateSwapChain(const SwapChainCreateInfo &create_info) override;
    void DestroySwapChain(SwapChain *swap_chain) override;

    Shader *CreateShader(ShaderType type, const Path &file_name, const char *entry_point = "main") override;
    void DestroyShader(Shader *shader_program) override;

    Pipeline *CreateGraphicsPipeline(const GraphicsPipelineCreateInfo &create_info) override;
    Pipeline *CreateComputePipeline(const ComputePipelineCreateInfo &create_info) override;
    void DestroyPipeline(Pipeline *pipeline) override;

    CommandList *GetCommandList(CommandQueueType type) override;

    Semaphore *CreateSemaphore1() override;
    void DestroySemaphore(Semaphore *semaphore) override;

    Sampler *CreateSampler(const SamplerDesc &sampler_desc) override;
    void DestroySampler(Sampler *sampler) override;

    void WaitGpuExecution(CommandQueueType queue_type) override;
    void ResetRHIResources() override;
    void ResetFence(CommandQueueType queue_type) override;

    void SubmitCommandLists(const QueueSubmitInfo &queue_submit_info) override;
    void AcquireNextFrame(SwapChain *swap_chain) override;
    void Present(const QueuePresentInfo &queue_present_info) override;

    double QueryResult() override;

  private:
    void CommitCommandBuffer(MTL::CommandBuffer *command_buffer) noexcept;

    MTL::Device *m_device{};
    MTL::CommandQueue *m_command_queue{};
    MetalCommandList *m_graphics_command_list{};
    MetalCommandList *m_pending_present_command_list{};
    MTL::CommandBuffer *m_last_committed_command_buffer{};
};

std::unique_ptr<RHI> CreateMetalRenderBackend(bool offscreen) noexcept;

} // namespace Horizon::Backend
