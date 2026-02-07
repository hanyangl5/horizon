#include "rhi_dx12.h"

#include <core/path.h>
#include <core/memory.h>
#include <core/log.h>
#include <core/glfwwindow.h>

#include "dx12_buffer.h"
#include "dx12_texture.h"
#include "dx12_render_target.h"
#include "dx12_swap_chain.h"
#include "dx12_command_context.h"
#include "dx12_descriptor_heap_allocator.h"
#include "dx12_shader.h"
#include "dx12_shader_compiler.h"
#include "dx12_pipeline.h"
#include "dx12_utils.h"
#include "dx12_config.h"

#ifdef _WIN32
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")


using Microsoft::WRL::ComPtr;
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

    // Cleanup command context
    Memory::Free(thread_command_context);
    thread_command_context = nullptr;

    // Cleanup descriptor heap allocator
    Memory::Free(m_descriptor_heap_allocator);
    m_descriptor_heap_allocator = nullptr;

    // Close fence event
    if (m_dx12.fence_event != nullptr)
    {
        CloseHandle(m_dx12.fence_event);
        m_dx12.fence_event = nullptr;
    }
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
}

void RHIDX12::CreateFactory()
{
    UINT dxgi_factory_flags = 0;

#ifdef _DEBUG
    // Enable debug layer
    {
        ComPtr<ID3D12Debug> debug_controller;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_controller))))
        {
            debug_controller->EnableDebugLayer();
            dxgi_factory_flags |= DXGI_CREATE_FACTORY_DEBUG;
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
    ComPtr<IDXGIAdapter1> adapter;
    ComPtr<IDXGIFactory6> factory6;
    
    if (SUCCEEDED(m_dx12.factory.As(&factory6)))
    {
        // Prefer high-performance adapter
        for (UINT adapter_index = 0;
             SUCCEEDED(factory6->EnumAdapterByGpuPreference(adapter_index, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                                                            IID_PPV_ARGS(&adapter)));
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
                LOG_INFO("Selected DX12 adapter: {}", (void*)desc.Description);
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
            LOG_INFO("Selected DX12 adapter: {}", (void*)desc.Description);
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
    m_dx12.sampler_descriptor_size = m_dx12.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

#ifdef _DEBUG
    // Enable debug messages
    ComPtr<ID3D12InfoQueue> info_queue;
    if (SUCCEEDED(m_dx12.device.As(&info_queue)))
    {
        info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
        info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
        info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);
    }
#endif
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
    return Memory::Alloc<DX12RenderTarget>(m_dx12, render_target_create_info);
}

SwapChain *RHIDX12::CreateSwapChain(const SwapChainCreateInfo &create_info)
{
    return Memory::Alloc<DX12SwapChain>(m_dx12, create_info, m_window);
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
    std::vector<u8> bytecode = DX12ShaderCompiler::CompileHLSL(file_name, type, entry_point, shader_dir);
    if (bytecode.empty())
    {
        LOG_ERROR("Failed to compile DX12 shader: {}", (void*)file_name.c_str());
        return nullptr;
    }

    return Memory::Alloc<DX12Shader>(m_dx12, type, bytecode, entry_point);
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
        thread_command_context = Memory::Alloc<DX12CommandContext>(m_dx12);
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
    // TODO: Implement DX12 semaphore/fence creation
    LOG_ERROR("DX12 semaphore creation not yet implemented");
    return nullptr;
}

Sampler *RHIDX12::CreateSampler(const SamplerDesc &sampler_desc)
{
    // TODO: Implement DX12 sampler creation
    LOG_ERROR("DX12 sampler creation not yet implemented");
    return nullptr;
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
    // TODO: Implement DX12 semaphore destruction
    if (semaphore != nullptr)
    {
        Memory::Free(semaphore);
    }
}

void RHIDX12::DestroySampler(Sampler *sampler)
{
    // TODO: Implement DX12 sampler destruction
    if (sampler != nullptr)
    {
        Memory::Free(sampler);
    }
}

void RHIDX12::SubmitCommandLists(const QueueSubmitInfo &queue_submit_info)
{
    // TODO: Implement DX12 command list submission
    LOG_ERROR("DX12 command list submission not yet implemented");
}

void RHIDX12::Present(const QueuePresentInfo &queue_present_info)
{
    // TODO: Implement DX12 present
    LOG_ERROR("DX12 present not yet implemented");
}

void RHIDX12::AcquireNextFrame(SwapChain *swap_chain)
{
    // TODO: Implement DX12 frame acquisition
    LOG_ERROR("DX12 frame acquisition not yet implemented");
}

void RHIDX12::DestroySwapChain()
{
    // TODO: Implement swap chain destruction
}

} // namespace Horizon::Backend
