//#pragma once
//
//
//
//
//#include <D3D12MemAlloc.h>
//
//#include <runtime/core/log/log.h>
//#include <runtime/core/utils/definations.h>
//#include <runtime/function/rhi/pipeline.h>
//#include <runtime/function/rhi/rhi.h>
//#include <runtime/function/rhi/rhi_utils.h>
//#include <runtime/function/rhi/Shader.h>
//#include <runtime/function/rhi/dx12/DX12Buffer.h>
//#include <runtime/function/rhi/dx12/DX12CommandContext.h>
//#include <runtime/function/rhi/dx12/DX12Texture.h>
//
// namespace Horizon::Backend {
//
// class RHIDX12 : public RHI {
//  public:
//    RHIDX12() noexcept;
//    virtual ~RHIDX12() noexcept;
//
//    void InitializeRenderer() noexcept override;
//
//    Buffer*
//    CreateBuffer(const BufferCreateInfo &create_info) noexcept override;
//
//    Texture* CreateTexture(
//        const TextureCreateInfo &texture_create_info) noexcept override;
//
//    void CreateSwapChain(Window *window) noexcept override;
//
//    Shader *CreateShader(ShaderType type,
//                                       const Container::String &entry_point,
//                                       u32 compile_flags,
//                                       Container::String file_name) noexcept
//                                       override;
//
//    void DestroyShader(Shader *shader_program) noexcept
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
//        Container::Array<CommandList *> &command_lists) noexcept override;
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
//        *transfer_queue; Container::FixedArray<ID3D12CommandQueue *, 3> queues;
//        Container::FixedArray<ID3D12Fence *, 3> fences;
//        IDXGISwapChain3 *swap_chain;
//    } m_dx12{};
//};
//} // namespace Horizon::Backend
