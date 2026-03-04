#include "rhi_dx12.h"

#include <core/glfwwindow.h>
#include <core/log.h>
#include <core/memory.h>
#include <core/path.h>

#include "dx12_buffer.h"
#include "dx12_command_context.h"
#include "dx12_command_list.h"
#include "dx12_config.h"
#include "dx12_descriptor_heap_allocator.h"
#include "dx12_pipeline.h"
#include "dx12_render_target.h"
#include "dx12_sampler.h"
#include "dx12_semaphore.h"
#include "dx12_shader.h"
#include "dx12_shader_compiler.h"
#include "dx12_swap_chain.h"
#include "dx12_texture.h"
#include "dx12_utils.h"

#ifdef _WIN32
#include <DirectXHelpers.h>
#include <d3d12.h>
#include <d3d12sdklayers.h> // D3D12 SDK Layers for debugging
#include <dxgi1_6.h>
#include <windows.h>
#include <wrl/client.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

#endif

namespace Horizon::Backend
{

std::unique_ptr<RHI> CreateDX12RenderBackend(bool offscreen) noexcept
{
    return std::make_unique<RHIDX12>(offscreen);
}

RHIDX12::RHIDX12(bool offscreen) noexcept
{
    m_offscreen = offscreen;
}

RHIDX12::~RHIDX12() noexcept
{
    // Wait for GPU to finish all work
    for (u32 i = 0; i < 3; ++i)
    {
        WaitForGPU(static_cast<CommandQueueType>(i));
    }

    // Release process-lifetime DX12 helpers (e.g. runtime mip-gen PSO/root signature)
    // before destroying the device to avoid false-positive live-object reports on exit.
    ShutdownDX12CommandListGlobals();

    // Cleanup DXC compiler
    DX12ShaderCompiler::CleanupDXC();

    // Cleanup command context
    if (thread_command_context)
    {
        Memory::Free(thread_command_context);
        thread_command_context = nullptr;
    }

    // Cleanup descriptor heap allocator
    if (m_descriptor_heap_allocator)
    {
        Memory::Free(m_descriptor_heap_allocator);
        m_descriptor_heap_allocator = nullptr;
    }

    // Close fence event
    if (m_dx12.fence_event != nullptr)
    {
        CloseHandle(m_dx12.fence_event);
        m_dx12.fence_event = nullptr;
    }

    // ComPtr will automatically release all DX12 resources (device, queues, fences, etc.)
}

void RHIDX12::InitializeRenderer()
{
    LOG_DEBUG("using DirectX 12 renderer");
    InitializeDX12Renderer("DirectX 12 Renderer");
}

void RHIDX12::InitializeDX12Renderer(const std::string &app_name)
{
    CreateFactory();
    PickAdapter();
    CreateDevice();
    CreateCommandQueues();
    CreateFences();
    CreateDescriptorHeaps();

    // Create command signature for indirect indexed instanced drawing
    {
        D3D12_INDIRECT_ARGUMENT_DESC arg_desc{};
        arg_desc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;

        D3D12_COMMAND_SIGNATURE_DESC cmd_sig_desc{};
        cmd_sig_desc.ByteStride = sizeof(D3D12_DRAW_INDEXED_ARGUMENTS); // 20 bytes
        cmd_sig_desc.NumArgumentDescs = 1;
        cmd_sig_desc.pArgumentDescs = &arg_desc;
        cmd_sig_desc.NodeMask = 0;

        HRESULT hr = m_dx12.device->CreateCommandSignature(
            &cmd_sig_desc, nullptr, IID_PPV_ARGS(&m_dx12.draw_indexed_indirect_command_signature));
        if (FAILED(hr))
        {
            LOG_ERROR("Failed to create draw indexed indirect command signature: {}", hr);
        }
    }
}

void RHIDX12::CreateFactory()
{
    UINT dxgi_factory_flags = 0;

#ifdef _DEBUG
    // Enable D3D12 debug layer
    {
        Microsoft::WRL::ComPtr<ID3D12Debug> debug_controller;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_controller))))
        {
            debug_controller->EnableDebugLayer();
            LOG_DEBUG("D3D12 Debug Layer enabled");

            // Enable GPU-based validation for more thorough debugging
            Microsoft::WRL::ComPtr<ID3D12Debug1> debug_controller1;
            if (SUCCEEDED(debug_controller.As(&debug_controller1)))
            {
                debug_controller1->EnableDebugLayer();
                // GPU-based validation is very slow, enable only if needed
                debug_controller1->SetEnableGPUBasedValidation(TRUE);
                LOG_DEBUG("D3D12 Debug Layer 1 enabled");
            }

            dxgi_factory_flags |= DXGI_CREATE_FACTORY_DEBUG;
        }
        else
        {
            LOG_WARN("Failed to enable D3D12 Debug Layer. Install Graphics Tools from Windows Store for full debugging "
                     "support.");
        }
    }
#endif

    HRESULT hr = CreateDXGIFactory2(dxgi_factory_flags, IID_PPV_ARGS(&m_dx12.factory));
    if (FAILED(hr))
    {
        LOG_ERROR("Failed to create DXGI factory: {}", hr);
        return;
    }
}

void RHIDX12::PickAdapter()
{
    Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
    Microsoft::WRL::ComPtr<IDXGIFactory6> factory6;

    if (SUCCEEDED(m_dx12.factory.As(&factory6)))
    {
        // Prefer high-performance adapter
        for (UINT adapter_index = 0; SUCCEEDED(factory6->EnumAdapterByGpuPreference(
                 adapter_index, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter)));
             ++adapter_index)
        {
            DXGI_ADAPTER_DESC1 desc;
            adapter->GetDesc1(&desc);

            // Skip software adapters
            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            {
                continue;
            }

            // Check if adapter supports D3D12
            if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), DX12_FEATURE_LEVEL, _uuidof(ID3D12Device), nullptr)))
            {
                m_dx12.adapter = adapter;
                LOG_INFO("Selected DX12 adapter: {}", (void *)desc.Description);
                return;
            }
        }
    }

    // Fallback to first available adapter
    for (UINT adapter_index = 0; SUCCEEDED(m_dx12.factory->EnumAdapters1(adapter_index, &adapter)); ++adapter_index)
    {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);

        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
        {
            continue;
        }

        if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), DX12_FEATURE_LEVEL, _uuidof(ID3D12Device), nullptr)))
        {
            m_dx12.adapter = adapter;
            LOG_INFO("Selected DX12 adapter: {}", (void *)desc.Description);
            return;
        }
    }

    LOG_ERROR("Failed to find suitable DX12 adapter");
}

void RHIDX12::CreateDevice()
{
    HRESULT hr = D3D12CreateDevice(m_dx12.adapter.Get(), DX12_FEATURE_LEVEL, IID_PPV_ARGS(&m_dx12.device));
    if (FAILED(hr))
    {
        LOG_ERROR("Failed to create D3D12 device: {}", hr);
        return;
    }

    // Get descriptor sizes
    m_dx12.rtv_descriptor_size = m_dx12.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    m_dx12.dsv_descriptor_size = m_dx12.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    m_dx12.srv_uav_descriptor_size =
        m_dx12.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    m_dx12.cbv_descriptor_size = m_dx12.srv_uav_descriptor_size; // Same heap type
    m_dx12.sampler_descriptor_size =
        m_dx12.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

#ifdef _DEBUG
    // Configure debug message queue
    Microsoft::WRL::ComPtr<ID3D12InfoQueue> info_queue;
    if (SUCCEEDED(m_dx12.device.As(&info_queue)))
    {
        // Set break on severity levels
        info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
        info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
        // Optionally break on warnings (can be very verbose)
        // info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);

        // Filter out known warnings/errors that are not critical
        D3D12_MESSAGE_ID hide[] = {
            // Add message IDs to hide here if needed
            D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
        };

        D3D12_INFO_QUEUE_FILTER filter = {};
        filter.DenyList.NumIDs = static_cast<UINT>(std::size(hide));
        filter.DenyList.pIDList = hide;
        if (filter.DenyList.NumIDs > 0)
        {
            info_queue->AddStorageFilterEntries(&filter);
        }

        // Log all messages
        LOG_DEBUG("D3D12 Info Queue configured for debugging");

        // Optional: Log message count
        UINT64 message_count = info_queue->GetNumStoredMessages();
        if (message_count > 0)
        {
            LOG_WARN("D3D12 Info Queue has {} stored messages", message_count);
        }
    }
    else
    {
        LOG_WARN("Failed to get D3D12 Info Queue interface");
    }
#endif

    D3D12_FEATURE_DATA_D3D12_OPTIONS7 options7{};
    hr = m_dx12.device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS7, &options7, sizeof(options7));
    m_mesh_shader_supported =
        SUCCEEDED(hr) && (options7.MeshShaderTier != D3D12_MESH_SHADER_TIER_NOT_SUPPORTED);
    LOG_INFO("DX12 mesh shader support: {}", m_mesh_shader_supported ? "enabled" : "disabled");
}

void RHIDX12::CreateCommandQueues()
{
    D3D12_COMMAND_QUEUE_DESC queue_desc = {};
    queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    // Graphics queue
    queue_desc.Type = Horizon::ToDX12CommandListType(CommandQueueType::GRAPHICS);
    HRESULT hr = m_dx12.device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&m_dx12.command_queues[GRAPHICS]));
    if (FAILED(hr))
    {
        LOG_ERROR("Failed to create graphics command queue: {}", hr);
        return;
    }

    // Compute queue
    queue_desc.Type = Horizon::ToDX12CommandListType(CommandQueueType::COMPUTE);
    hr = m_dx12.device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&m_dx12.command_queues[COMPUTE]));
    if (FAILED(hr))
    {
        LOG_ERROR("Failed to create compute command queue: {}", hr);
        return;
    }

    // Transfer queue (DX12 uses COPY queue type)
    queue_desc.Type = Horizon::ToDX12CommandListType(CommandQueueType::TRANSFER);
    hr = m_dx12.device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&m_dx12.command_queues[TRANSFER]));
    if (FAILED(hr))
    {
        LOG_ERROR("Failed to create transfer command queue: {}", hr);
        return;
    }
}

void RHIDX12::CreateFences()
{
    for (u32 i = 0; i < 3; ++i)
    {
        HRESULT hr = m_dx12.device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_dx12.fences[i]));
        if (FAILED(hr))
        {
            LOG_ERROR("Failed to create fence: {}", hr);
            return;
        }
        m_dx12.fence_values[i] = 0;
    }

    // Create fence event
    m_dx12.fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (m_dx12.fence_event == nullptr)
    {
        LOG_ERROR("Failed to create fence event: {}", GetLastError());
        return;
    }
}

void RHIDX12::CreateDescriptorHeaps()
{
    m_descriptor_heap_allocator = Memory::Alloc<DX12DescriptorHeapAllocator>(m_dx12);
}

void RHIDX12::WaitForGPU(CommandQueueType queue_type)
{
    u32 index = static_cast<u32>(queue_type);
    UINT64 fence_value = ++m_dx12.fence_values[index];
    HRESULT hr = m_dx12.command_queues[index]->Signal(m_dx12.fences[index].Get(), fence_value);
    if (FAILED(hr))
    {
        LOG_ERROR("Failed to signal fence: {}", hr);
        return;
    }

    if (m_dx12.fences[index]->GetCompletedValue() < fence_value)
    {
        hr = m_dx12.fences[index]->SetEventOnCompletion(fence_value, m_dx12.fence_event);
        if (FAILED(hr))
        {
            LOG_ERROR("Failed to set event on completion: {}", hr);
            return;
        }
        WaitForSingleObject(m_dx12.fence_event, INFINITE);
    }
}

Buffer *RHIDX12::CreateBuffer(const BufferCreateInfo &buffer_create_info)
{
    return Memory::Alloc<DX12Buffer>(m_dx12, buffer_create_info);
}

Texture *RHIDX12::CreateTexture(const TextureCreateInfo &texture_create_info)
{
    return Memory::Alloc<DX12Texture>(m_dx12, texture_create_info);
}

RenderTarget *RHIDX12::CreateRenderTarget(const RenderTargetCreateInfo &render_target_create_info)
{
    if (!m_descriptor_heap_allocator)
    {
        LOG_ERROR("Descriptor heap allocator not initialized");
        return nullptr;
    }

    // Create render target object
    auto *render_target = Memory::Alloc<DX12RenderTarget>(m_dx12, render_target_create_info);
    if (!render_target)
    {
        LOG_ERROR("Failed to allocate render target");
        return nullptr;
    }

    // Get the texture resource
    auto *dx12_texture = reinterpret_cast<DX12Texture *>(render_target->GetTexture());
    if (!dx12_texture || !dx12_texture->GetResource())
    {
        LOG_ERROR("Invalid texture for render target");
        return render_target; // Return even if texture is invalid
    }

    // Allocate and create RTV or DSV based on render target type
    if (render_target_create_info.rt_type == RenderTargetType::COLOR)
    {
        // Allocate RTV handle
        D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = m_descriptor_heap_allocator->AllocateRTV();

        // Create RTV (render targets are always 2D textures)
        D3D12_RENDER_TARGET_VIEW_DESC rtv_desc = {};
        rtv_desc.Format = ToDX12Format(render_target_create_info.rt_format);
        rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        rtv_desc.Texture2D.MipSlice = 0;

        m_dx12.device->CreateRenderTargetView(dx12_texture->GetResource(), &rtv_desc, rtv_handle);

        // Store the handle
        render_target->m_rtv_handle = rtv_handle;
    }
    else if (render_target_create_info.rt_type == RenderTargetType::DEPTH_STENCIL)
    {
        // Allocate DSV handle
        D3D12_CPU_DESCRIPTOR_HANDLE dsv_handle = m_descriptor_heap_allocator->AllocateDSV();

        // Create DSV (render targets are always 2D textures)
        D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc = {};
        dsv_desc.Format = ToDX12Format(render_target_create_info.rt_format);
        dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        dsv_desc.Flags = D3D12_DSV_FLAG_NONE;
        dsv_desc.Texture2D.MipSlice = 0;

        m_dx12.device->CreateDepthStencilView(dx12_texture->GetResource(), &dsv_desc, dsv_handle);

        // Store the handle
        render_target->m_dsv_handle = dsv_handle;
    }

    return render_target;
}

SwapChain *RHIDX12::CreateSwapChain(const SwapChainCreateInfo &create_info)
{
    if (!m_descriptor_heap_allocator)
    {
        LOG_ERROR("Descriptor heap allocator not initialized");
        return nullptr;
    }

    // Create swap chain
    auto *swap_chain = Memory::Alloc<DX12SwapChain>(m_dx12, create_info, m_window);
    if (!swap_chain)
    {
        LOG_ERROR("Failed to create swap chain");
        return nullptr;
    }

    // Create RTV handles for all back buffers
    auto *dx12_swap_chain = reinterpret_cast<DX12SwapChain *>(swap_chain);
    for (u32 i = 0; i < dx12_swap_chain->m_back_buffers.size(); ++i)
    {
        auto *render_target = dx12_swap_chain->render_targets[i];
        if (render_target)
        {
            auto *dx12_rt = reinterpret_cast<DX12RenderTarget *>(render_target);

            // Allocate RTV handle
            D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = m_descriptor_heap_allocator->AllocateRTV();

            // Create RTV for the back buffer
            m_dx12.device->CreateRenderTargetView(dx12_swap_chain->m_back_buffers[i].Get(), nullptr, rtv_handle);

            // Store the handle
            dx12_rt->m_rtv_handle = rtv_handle;
        }
    }

    return swap_chain;
}

Shader *RHIDX12::CreateShader(ShaderType type, const Path &file_name, const char *entry_point)
{
    // Determine shader directory
    Path shader_dir;
    if (file_name.is_absolute())
    {
        shader_dir = file_name.parent_path();
    }

    // Compile HLSL shader
    // TODO: precompile to ir
    IDxcBlob *reflection = nullptr;
    IDxcBlob *bytecode = DX12ShaderCompiler::CompileHLSL(file_name, type, entry_point, shader_dir, &reflection);
    if (!bytecode)
    {
        LOG_ERROR("Failed to compile DX12 shader: {}", (void *)file_name.c_str());
        if (reflection)
        {
            reflection->Release();
        }
        return nullptr;
    }

    return Memory::Alloc<DX12Shader>(m_dx12, type, bytecode, entry_point, reflection);
}

void RHIDX12::DestroyShader(Shader *shader_program)
{
    // TODO: Implement DX12 shader destruction
    if (shader_program != nullptr)
    {
        Memory::Free(shader_program);
    }
}

CommandList *RHIDX12::GetCommandList(CommandQueueType type)
{
    if (!thread_command_context)
    {
        thread_command_context = Memory::Alloc<DX12CommandContext>(m_dx12, m_descriptor_heap_allocator);
        m_command_context = thread_command_context;
    }
    return thread_command_context->GetCommandList(type);
}

void RHIDX12::WaitGpuExecution(CommandQueueType queue_type)
{
    WaitForGPU(queue_type);
}

void RHIDX12::ResetRHIResources()
{
    if (thread_command_context)
    {
        thread_command_context->Reset();
    }
    if (m_descriptor_heap_allocator)
    {
        m_descriptor_heap_allocator->ResetDescriptorHeaps();
    }
}

void RHIDX12::ResetFence(CommandQueueType queue_type)
{
    u32 index = static_cast<u32>(queue_type);
    m_dx12.fence_values[index] = 0;
    m_dx12.fences[index]->Signal(0);
}

Pipeline *RHIDX12::CreateGraphicsPipeline(const GraphicsPipelineCreateInfo &create_info)
{
    if (!m_descriptor_heap_allocator)
    {
        LOG_ERROR("Descriptor heap allocator not initialized");
        return nullptr;
    }
    return Memory::Alloc<DX12Pipeline>(m_dx12, create_info, *m_descriptor_heap_allocator);
}

Pipeline *RHIDX12::CreateComputePipeline(const ComputePipelineCreateInfo &create_info)
{
    if (!m_descriptor_heap_allocator)
    {
        LOG_ERROR("Descriptor heap allocator not initialized");
        return nullptr;
    }
    return Memory::Alloc<DX12Pipeline>(m_dx12, create_info, *m_descriptor_heap_allocator);
}

void RHIDX12::DestroyPipeline(Pipeline *pipeline)
{
    // TODO: Implement DX12 pipeline destruction
    if (pipeline != nullptr)
    {
        Memory::Free(pipeline);
    }
}

Semaphore *RHIDX12::CreateSemaphore1()
{
    return Memory::Alloc<DX12Semaphore>(m_dx12);
}

Sampler *RHIDX12::CreateSampler(const SamplerDesc &sampler_desc)
{
    if (!m_descriptor_heap_allocator)
    {
        LOG_ERROR("Descriptor heap allocator not initialized");
        return nullptr;
    }
    return Memory::Alloc<DX12Sampler>(m_dx12, *m_descriptor_heap_allocator, sampler_desc);
}

void RHIDX12::DestroyBuffer(Buffer *buffer)
{
    if (buffer != nullptr)
    {
        Memory::Free(buffer);
    }
}

void RHIDX12::DestroyTexture(Texture *texture)
{
    if (texture != nullptr)
    {
        Memory::Free(texture);
    }
}

void RHIDX12::DestroyRenderTarget(RenderTarget *render_target)
{
    if (render_target != nullptr)
    {
        Memory::Free(render_target);
    }
}

void RHIDX12::DestroySwapChain(SwapChain *swap_chain)
{
    if (swap_chain != nullptr)
    {
        Memory::Free(swap_chain);
    }
}

void RHIDX12::DestroySemaphore(Semaphore *semaphore)
{
    if (semaphore != nullptr)
    {
        Memory::Free(semaphore);
    }
}

void RHIDX12::DestroySampler(Sampler *sampler)
{
    if (sampler != nullptr)
    {
        Memory::Free(sampler);
    }
}

void RHIDX12::SubmitCommandLists(const QueueSubmitInfo &queue_submit_info)
{
    u32 queue_index = static_cast<u32>(queue_submit_info.queue_type);
    if (queue_index >= 3)
    {
        LOG_ERROR("Invalid queue type: {}", queue_index);
        return;
    }

    // Convert command lists to D3D12 command lists
    std::vector<ID3D12CommandList *> d3d12_command_lists;
    d3d12_command_lists.reserve(queue_submit_info.command_lists.size());

    for (auto *cmd_list : queue_submit_info.command_lists)
    {
        auto *dx12_cmd_list = reinterpret_cast<DX12CommandList *>(cmd_list);
        d3d12_command_lists.push_back(dx12_cmd_list->GetD3D12CommandList());
    }

    // Wait for semaphores (fences) if any
    for (auto *semaphore : queue_submit_info.wait_semaphores)
    {
        auto *dx12_semaphore = reinterpret_cast<DX12Semaphore *>(semaphore);
        UINT64 wait_value = dx12_semaphore->GetFenceValue();
        if (wait_value > 0)
        {
            // Wait for the fence to reach the specified value
            if (m_dx12.fences[queue_index]->GetCompletedValue() < wait_value)
            {
                HRESULT hr = m_dx12.fences[queue_index]->SetEventOnCompletion(wait_value, m_dx12.fence_event);
                if (SUCCEEDED(hr))
                {
                    WaitForSingleObject(m_dx12.fence_event, INFINITE);
                }
            }
        }
    }

    // Handle image acquired semaphore if needed
    if (queue_submit_info.wait_image_acquired)
    {
        // Wait for present complete semaphore (if exists)
        // This is typically handled by the swap chain
    }

    // Execute command lists
    if (!d3d12_command_lists.empty())
    {
        m_dx12.command_queues[queue_index]->ExecuteCommandLists(static_cast<UINT>(d3d12_command_lists.size()),
                                                                d3d12_command_lists.data());
    }

    // Signal semaphores if any
    for (auto *semaphore : queue_submit_info.signal_semaphores)
    {
        auto *dx12_semaphore = reinterpret_cast<DX12Semaphore *>(semaphore);
        UINT64 signal_value = ++m_dx12.fence_values[queue_index];
        dx12_semaphore->SetFenceValue(signal_value);
        m_dx12.command_queues[queue_index]->Signal(dx12_semaphore->GetFence(), signal_value);
    }

    // Signal render complete semaphore if needed
    if (queue_submit_info.signal_render_complete)
    {
        // This is typically handled by the swap chain
    }

    // Update fence value for the queue
    UINT64 fence_value = ++m_dx12.fence_values[queue_index];
    m_dx12.command_queues[queue_index]->Signal(m_dx12.fences[queue_index].Get(), fence_value);
}

void RHIDX12::Present(const QueuePresentInfo &queue_present_info)
{
    auto *dx12_swap_chain = reinterpret_cast<DX12SwapChain *>(queue_present_info.swap_chain);
    if (!dx12_swap_chain)
    {
        LOG_ERROR("Invalid swap chain for present");
        return;
    }

    // Present the swap chain
    HRESULT hr = dx12_swap_chain->m_swap_chain->Present(1, 0); // VSync enabled
    if (FAILED(hr))
    {
        LOG_ERROR("Failed to present swap chain: {}", hr);
        return;
    }

    // Update frame index
    dx12_swap_chain->current_frame_index =
        (dx12_swap_chain->current_frame_index + 1) % dx12_swap_chain->m_back_buffer_count;
}

void RHIDX12::AcquireNextFrame(SwapChain *swap_chain)
{
    auto *dx12_swap_chain = reinterpret_cast<DX12SwapChain *>(swap_chain);
    if (!dx12_swap_chain)
    {
        LOG_ERROR("Invalid swap chain for frame acquisition");
        return;
    }

    if (!m_descriptor_heap_allocator)
    {
        LOG_ERROR("Descriptor heap allocator not initialized");
        return;
    }

    if (m_window && m_window->GetWidth() > 0 && m_window->GetHeight() > 0 &&
        (dx12_swap_chain->width != m_window->GetWidth() || dx12_swap_chain->height != m_window->GetHeight()))
    {
        WaitForGPU(CommandQueueType::GRAPHICS);
        if (!dx12_swap_chain->Resize(m_window->GetWidth(), m_window->GetHeight()))
        {
            return;
        }
    }

    // Get current back buffer index
    dx12_swap_chain->image_index = dx12_swap_chain->m_swap_chain->GetCurrentBackBufferIndex();
    dx12_swap_chain->current_frame_index = dx12_swap_chain->image_index;

    // Get or create render target for this back buffer
    auto *render_target = dx12_swap_chain->render_targets[dx12_swap_chain->image_index];

    if (render_target == nullptr)
    {
        // Create RTV for the back buffer
        D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = m_descriptor_heap_allocator->AllocateRTV();
        m_dx12.device->CreateRenderTargetView(dx12_swap_chain->m_back_buffers[dx12_swap_chain->image_index].Get(),
                                              nullptr, rtv_handle);

        // Create render target wrapper for the back buffer
        dx12_swap_chain->render_targets[dx12_swap_chain->image_index] = Memory::Alloc<DX12RenderTarget>(
            m_dx12, dx12_swap_chain->m_back_buffers[dx12_swap_chain->image_index], rtv_handle);
    }
    else
    {
        // Check if RTV handle is valid, if not, create it
        auto *dx12_rt = reinterpret_cast<DX12RenderTarget *>(render_target);
        if (dx12_rt->GetRTVHandle().ptr == 0)
        {
            // Allocate and create RTV handle
            D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = m_descriptor_heap_allocator->AllocateRTV();
            m_dx12.device->CreateRenderTargetView(dx12_swap_chain->m_back_buffers[dx12_swap_chain->image_index].Get(),
                                                  nullptr, rtv_handle);

            // Store the handle
            dx12_rt->m_rtv_handle = rtv_handle;
        }
    }
}

void RHIDX12::DestroySwapChain()
{
    // TODO: Implement swap chain destruction
}

} // namespace Horizon::Backend
