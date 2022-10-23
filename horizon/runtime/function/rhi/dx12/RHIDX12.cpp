//#include <runtime/function/rhi/dx12/DX12Config.h>
//#include <runtime/function/rhi/dx12/RHIDX12.h>
//
//namespace Horizon::Backend {
//
//RHIDX12::RHIDX12() noexcept {}
//
//RHIDX12::~RHIDX12() noexcept {
//    m_dx12.d3dma_allocator->Release();
//    m_dx12.active_gpu->Release();
//    m_dx12.device->Release();
//    m_dx12.factory->Release();
//}
//
//void RHIDX12::InitializeRenderer() noexcept {
//    LOG_DEBUG("using DirectX 12 renderer");
//    InitializeDX12Renderer();
//}
//
//Resource<Buffer>
//RHIDX12::CreateBuffer(const BufferCreateInfo &create_info) noexcept {
//    return std::make_unique<DX12Buffer>(m_dx12.d3dma_allocator, create_info,
//                                        MemoryFlag::DEDICATE_GPU_MEMORY);
//}
//
//Resource<Texture>
//RHIDX12::CreateTexture(const TextureCreateInfo &texture_create_info) noexcept {
//    return std::make_unique<DX12Texture>(m_dx12.d3dma_allocator,
//                                         texture_create_info);
//}
//
//void RHIDX12::CreateSwapChain(Window *window) noexcept {
//    DXGI_SWAP_CHAIN_DESC1 swap_chain_desc{};
//    swap_chain_desc.BufferCount = m_back_buffer_count;
//    swap_chain_desc.Width = window->GetWidth();
//    swap_chain_desc.Height = window->GetHeight();
//    swap_chain_desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
//    swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
//    swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
//    swap_chain_desc.SampleDesc.Count = 1;
//    swap_chain_desc.SampleDesc.Quality = 0;
//    swap_chain_desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
//
//    IDXGISwapChain1 *swap_chain{};
//
//    CHECK_DX_RESULT(m_dx12.factory->CreateSwapChainForHwnd(
//        m_dx12.queues[CommandQueueType::GRAPHICS], // Swap chain needs the queue
//                                                   // so that it can
//                                                   // force a flush on it.
//        window->GetWin32Window(), &swap_chain_desc, nullptr, nullptr,
//        &swap_chain));
//
//    m_dx12.swap_chain = static_cast<IDXGISwapChain3 *>(swap_chain);
//    m_current_frame_index = m_dx12.swap_chain->GetCurrentBackBufferIndex();
//}
//
//Shader *RHIDX12::CreateShader(ShaderType type,
//                                            const std::string &entry_point,
//                                            u32 compile_flags,
//                                            std::string file_name) noexcept {
//    auto dxil_blob{m_shader_compiler->CompileFromFile(
//        ShaderTargetPlatform::DXIL, type, entry_point, compile_flags,
//        file_name)};
//
//    // TODO: DX12 shader program
//    return new Shader(type, entry_point);
//}
//
//void RHIDX12::DestroyShader(Shader *shader_program) noexcept {
//    if (shader_program) {
//        delete shader_program;
//    } else {
//        LOG_WARN("shader program is uninitialized or deleted");
//    }
//}
//
//CommandList *RHIDX12::GetCommandList(CommandQueueType type) noexcept {
//    if (!thread_command_context) {
//        thread_command_context =
//            std::make_unique<DX12CommandContext>(m_dx12.device);
//    }
//
//    return thread_command_context->GetCommandList(type);
//}
//
//void RHIDX12::WaitGpuExecution(CommandQueueType queue_type) noexcept {
//    // m_dx12.queues[queue_type]->Wait();
//}
//
//void RHIDX12::ResetCommandResources() noexcept {
//    if (thread_command_context) {
//        thread_command_context->Reset();
//    }
//}
//
//Pipeline *RHIDX12::CreatePipeline(
//    const PipelineCreateInfo &pipeline_create_info) noexcept {
//    return nullptr;
//}
//
//void RHIDX12::SubmitCommandLists(
//    CommandQueueType queue_type,
//    std::vector<CommandList *> &command_lists) noexcept {
//    // submit command lists
//    // submit command lists
//    // for (auto& command_list : *command_list) {
//    // 	command_list->Submit();
//    // }
//}
//
//void RHIDX12::SetResource(Buffer *buffer, Pipeline *pipeline, u32 set,
//                          u32 binding) noexcept {}
//
//void RHIDX12::SetResource(Texture *texture) noexcept {}
//
//void RHIDX12::UpdateDescriptors() noexcept {}
//
//void RHIDX12::InitializeDX12Renderer() noexcept {
//    CreateFactory();
//    CreateDevice();
//    InitializeD3DMA();
//}
//
//void RHIDX12::CreateFactory() noexcept {
//    u32 dxgi_factory_flags = 0;
//#ifndef NDEBUG
//    dxgi_factory_flags |= DXGI_CREATE_FACTORY_DEBUG;
//#endif
//    CHECK_DX_RESULT(
//        CreateDXGIFactory2(dxgi_factory_flags, IID_PPV_ARGS(&m_dx12.factory)));
//}
//
//void RHIDX12::PickGPU(IDXGIFactory6 *factory, IDXGIAdapter4 **gpu) noexcept {
//    u32 gpu_count = 0;
//    IDXGIAdapter4 *adapter = NULL;
//    IDXGIFactory6 *factory6;
//
//    CHECK_DX_RESULT(factory->QueryInterface(IID_PPV_ARGS(&factory6)))
//
//    // Find number of usable GPUs
//    // Use DXGI6 interface which lets us specify gpu preference so we dont need
//    // to use NVOptimus or AMDPowerExpress exports
//    for (u32 i = 0;
//         DXGI_ERROR_NOT_FOUND !=
//         factory6->EnumAdapterByGpuPreference(
//             i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter));
//         ++i) {
//        DXGI_ADAPTER_DESC3 desc;
//        adapter->GetDesc3(&desc);
//        if (SUCCEEDED(D3D12CreateDevice(adapter, DX12_API_VERSION,
//                                        _uuidof(ID3D12Device), nullptr))) {
//            break;
//        }
//    }
//    *gpu = adapter;
//}
//
//void RHIDX12::CreateDevice() noexcept {
//
//    // pick gpu
//    PickGPU(m_dx12.factory, &m_dx12.active_gpu);
//
//    CHECK_DX_RESULT(D3D12CreateDevice(m_dx12.active_gpu, DX12_API_VERSION,
//                                      IID_PPV_ARGS(&m_dx12.device)));
//
//    // create command queue
//
//    D3D12_COMMAND_QUEUE_DESC graphics_command_queue_desc{};
//    graphics_command_queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
//    graphics_command_queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
//
//    CHECK_DX_RESULT(m_dx12.device->CreateCommandQueue(
//        &graphics_command_queue_desc,
//        IID_PPV_ARGS(&m_dx12.queues[CommandQueueType::GRAPHICS])));
//
//#ifdef USE_ASYNC_COMPUTE_TRANSFER
//    D3D12_COMMAND_QUEUE_DESC compute_command_queue_desc{};
//    compute_command_queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
//    compute_command_queue_desc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
//
//    CHECK_DX_RESULT(m_dx12.device->CreateCommandQueue(
//        &compute_command_queue_desc,
//        IID_PPV_ARGS(&m_dx12.queues[CommandQueueType::COMPUTE])));
//
//    D3D12_COMMAND_QUEUE_DESC transfer_command_queue_desc{};
//    transfer_command_queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
//    transfer_command_queue_desc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
//
//    CHECK_DX_RESULT(m_dx12.device->CreateCommandQueue(
//        &transfer_command_queue_desc,
//        IID_PPV_ARGS(&m_dx12.queues[CommandQueueType::TRANSFER])));
//    LOG_INFO("using async compute & transfer");
//#endif // USE_ASYNC_COMPUTE_TRANSFER
//}
//
//void RHIDX12::InitializeD3DMA() noexcept {
//    D3D12MA::ALLOCATOR_DESC allocatorDesc{};
//    allocatorDesc.pDevice = m_dx12.device;
//    allocatorDesc.pAdapter = m_dx12.active_gpu;
//
//    CHECK_DX_RESULT(
//        D3D12MA::CreateAllocator(&allocatorDesc, &m_dx12.d3dma_allocator));
//}
//
//} // namespace Horizon::Backend