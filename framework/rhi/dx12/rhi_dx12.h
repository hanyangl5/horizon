#pragma once

#include <core/definations.h>
#include <core/path.h>

#include "dx12_descriptor_heap_allocator.h"
#include "dx12_utils.h"
#include <rhi/enums.h>
#include <rhi/rhi.h>
// Forward declarations
class DX12Buffer;
class DX12Texture;
class DX12RenderTarget;
class DX12SwapChain;

namespace Horizon::Backend
{

class RHIDX12 : public RHI
{
  public:
    RHIDX12(bool offscreen) noexcept;
    virtual ~RHIDX12() noexcept;

    RHIDX12(const RHIDX12 &rhs) noexcept = delete;
    RHIDX12 &operator=(const RHIDX12 &rhs) noexcept = delete;
    RHIDX12(RHIDX12 &&rhs) noexcept = delete;
    RHIDX12 &operator=(RHIDX12 &&rhs) noexcept = delete;

    void InitializeRenderer() override;

    Buffer *CreateBuffer(const BufferCreateInfo &buffer_create_info) override;

    Texture *CreateTexture(const TextureCreateInfo &texture_create_info) override;

    RenderTarget *CreateRenderTarget(const RenderTargetCreateInfo &render_target_create_info) override;

    SwapChain *CreateSwapChain(const SwapChainCreateInfo &create_info) override;

    Shader *CreateShader(ShaderType type, const Path &file_name, const char *entry_point) override;

    void DestroyShader(Shader *shader_program) override;

    CommandList *GetCommandList(CommandQueueType type) override;

    void WaitGpuExecution(CommandQueueType queue_type) override;

    void ResetRHIResources() override;

    void ResetFence(CommandQueueType queue_type) override;

    Pipeline *CreateGraphicsPipeline(const GraphicsPipelineCreateInfo &create_info) override;

    Pipeline *CreateComputePipeline(const ComputePipelineCreateInfo &create_info) override;

    void DestroyPipeline(Pipeline *pipeline) override;

    Semaphore *CreateSemaphore1() override;

    Sampler *CreateSampler(const SamplerDesc &sampler_desc) override;

    void DestroyBuffer(Buffer *buffer) override;

    void DestroyTexture(Texture *texture) override;

    void DestroyRenderTarget(RenderTarget *render_target) override;

    void DestroySwapChain(SwapChain *swap_chain) override;

    void DestroySemaphore(Semaphore *semaphore) override;

    void DestroySampler(Sampler *sampler) override;

    // submit command list to command queue
    void SubmitCommandLists(const QueueSubmitInfo &queue_submit_info) override;

    void Present(const QueuePresentInfo &queue_present_info) override;
    void AcquireNextFrame(SwapChain *swap_chain) override;
    bool SupportsMeshShader() const noexcept override
    {
        return m_mesh_shader_supported;
    }

  private:
    CommandContext *m_command_context{nullptr};
    void InitializeDX12Renderer(const std::string &app_name);
    void CreateFactory();
    void PickAdapter();
    void CreateDevice();
    void CreateCommandQueues();
    void CreateFences();
    void CreateDescriptorHeaps();
    void DestroySwapChain();
    void WaitForGPU(CommandQueueType queue_type);

    double QueryResult() override
    {
        // TODO: Implement GPU timestamp query for DX12
        return 0.0;
    }

  private:
    DX12RendererContext m_dx12{};
    SwapChainSemaphoreContext semaphore_ctx{};
    DX12DescriptorHeapAllocator *m_descriptor_heap_allocator{nullptr};
    bool m_mesh_shader_supported{false};
};

extern std::unique_ptr<RHI> CreateDX12RenderBackend(bool offscreen) noexcept;
} // namespace Horizon::Backend
