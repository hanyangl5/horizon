//#pragma once
//
//#include <string>
//#include <vector>
//
//#include <D3D12MemAlloc.h>
//
//#include <runtime/core/log/Log.h>
//#include <runtime/core/utils/Definations.h>
//#include <runtime/function/rhi/Pipeline.h>
//#include <runtime/function/rhi/RHI.h>
//#include <runtime/function/rhi/RHIUtils.h>
//#include <runtime/function/rhi/ShaderProgram.h>
//#include <runtime/function/rhi/dx12/DX12Buffer.h>
//#include <runtime/function/rhi/dx12/DX12CommandContext.h>
//#include <runtime/function/rhi/dx12/DX12Texture.h>
//
// namespace Horizon::RHI {
//
// class RHIDX12 : public RHI {
//  public:
//    RHIDX12() noexcept;
//    virtual ~RHIDX12() noexcept;
//
//    void InitializeRenderer() noexcept override;
//
//    Resource<Buffer>
//    CreateBuffer(const BufferCreateInfo &create_info) noexcept override;
//
//    Resource<Texture> CreateTexture(
//        const TextureCreateInfo &texture_create_info) noexcept override;
//
//    void CreateSwapChain(Window *window) noexcept override;
//
//    ShaderProgram *CreateShaderProgram(ShaderType type,
//                                       const std::string &entry_point,
//                                       u32 compile_flags,
//                                       std::string file_name) noexcept
//                                       override;
//
//    void DestroyShaderProgram(ShaderProgram *shader_program) noexcept
//    override;
//
//    CommandList *GetCommandList(CommandQueueType type) noexcept override;
//
//    void WaitGpuExecution(CommandQueueType queue_type) noexcept override;
//
//    void ResetCommandResources() noexcept override;
//
//    Pipeline *CreatePipeline(
//        const PipelineCreateInfo &pipeline_create_info) noexcept override;
//
//    // submit command list to command queue
//    void SubmitCommandLists(
//        CommandQueueType queue_type,
//        std::vector<CommandList *> &command_lists) noexcept override;
//
//    void SetResource(Buffer *buffer, Pipeline *pipeline, u32 set,
//                     u32 binding) noexcept override;
//    void SetResource(Texture *texture) noexcept override;
//    void UpdateDescriptors() noexcept override;
//
//  private:
//    void InitializeDX12Renderer() noexcept;
//    void CreateFactory() noexcept;
//    void PickGPU(IDXGIFactory6 *factory, IDXGIAdapter4 **gpu) noexcept;
//    void CreateDevice() noexcept;
//    void InitializeD3DMA() noexcept;
//
//  private:
//    struct DX12RendererContext {
//        ID3D12Device *device;
//        IDXGIFactory6 *factory;
//        IDXGIAdapter4 *active_gpu;
//        D3D12MA::Allocator *d3dma_allocator;
//        // ID3D12CommandQueue *graphics_queue, *compute_queue,
//        *transfer_queue; std::array<ID3D12CommandQueue *, 3> queues;
//        std::array<ID3D12Fence *, 3> fences;
//        IDXGISwapChain3 *swap_chain;
//    } m_dx12{};
//};
//} // namespace Horizon::RHI
