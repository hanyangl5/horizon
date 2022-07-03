#pragma once

#include <string>
#include <vector>

#include <D3D12MemAlloc.h>

#include <runtime/core/log/Log.h>
#include <runtime/core/utils/Definations.h>
#include <runtime/function/rhi/Pipeline.h>
#include <runtime/function/rhi/RHIInterface.h>
#include <runtime/function/rhi/RHIUtils.h>
#include <runtime/function/rhi/ShaderProgram.h>
#include <runtime/function/rhi/dx12/DX12Buffer.h>
#include <runtime/function/rhi/dx12/DX12CommandContext.h>
#include <runtime/function/rhi/dx12/DX12Texture.h>

namespace Horizon::RHI {

class RHIDX12 : public RHIInterface {
  public:
    RHIDX12() noexcept;
    virtual ~RHIDX12() noexcept;

    virtual void InitializeRenderer() noexcept override;

    virtual Resource<Buffer>
    CreateBuffer(const BufferCreateInfo &create_info) noexcept override;

    virtual Resource<Texture> CreateTexture(
        const TextureCreateInfo &texture_create_info) noexcept override;

    virtual void
    CreateSwapChain(std::shared_ptr<Window> window) noexcept override;

    virtual ShaderProgram *
    CreateShaderProgram(ShaderType type, const std::string &entry_point,
                        u32 compile_flags,
                        std::string file_name) noexcept override;

    virtual void
    DestroyShaderProgram(ShaderProgram *shader_program) noexcept override;

    virtual CommandList *
    GetCommandList(CommandQueueType type) noexcept override;

    virtual void ResetCommandResources() noexcept override;

    virtual Pipeline *CreatePipeline(
        const PipelineCreateInfo &pipeline_create_info) noexcept override;
    // submit command list to command queue
    virtual void SubmitCommandLists(
        CommandQueueType queue_type,
        std::vector<CommandList *> &command_lists) noexcept override;

  private:
    void InitializeDX12Renderer() noexcept;
    void CreateFactory() noexcept;
    void PickGPU(IDXGIFactory6 *factory, IDXGIAdapter4 **gpu) noexcept;
    void CreateDevice() noexcept;
    void InitializeD3DMA() noexcept;

  private:
    struct DX12RendererContext {
        ID3D12Device *device;
        IDXGIFactory6 *factory;
        IDXGIAdapter4 *active_gpu;
        D3D12MA::Allocator *d3dma_allocator;
        ID3D12CommandQueue *graphics_queue, *compute_queue, *transfer_queue;
        IDXGISwapChain3 *swap_chain;
    } m_dx12{};
};
} // namespace Horizon::RHI
