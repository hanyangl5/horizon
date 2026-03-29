#include "dx12_swap_chain.h"
#include <core/log.h>
#include <core/memory.h>
#include <core/window.h>

#include "dx12_render_target.h"
#include "dx12_texture.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace Horizon::Backend
{

DX12SwapChain::DX12SwapChain(const DX12RendererContext &context, const SwapChainCreateInfo &swap_chain_create_info,
                             Window *window) noexcept
    : SwapChain(swap_chain_create_info, window), m_context(context)
{
    // Get window handle
#ifdef _WIN32
    HWND hwnd = window != nullptr ? reinterpret_cast<HWND>(window->GetNativeWindow()) : nullptr;
    if (hwnd == nullptr)
    {
        LOG_ERROR("Failed to get native Win32 window handle from SDL window");
        return;
    }

    DXGI_SWAP_CHAIN_DESC1 swap_chain_desc = {};
    swap_chain_desc.Width = width;
    swap_chain_desc.Height = height;
    swap_chain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swap_chain_desc.Stereo = FALSE;
    swap_chain_desc.SampleDesc.Count = 1;
    swap_chain_desc.SampleDesc.Quality = 0;
    swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_chain_desc.BufferCount = swap_chain_create_info.back_buffer_count;
    swap_chain_desc.Scaling = DXGI_SCALING_STRETCH;
    swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swap_chain_desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    swap_chain_desc.Flags = 0;

    Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain1;
    HRESULT hr = m_context.factory->CreateSwapChainForHwnd(m_context.command_queues[GRAPHICS].Get(), hwnd,
                                                           &swap_chain_desc, nullptr, nullptr, &swap_chain1);
    if (FAILED(hr))
    {
        LOG_ERROR("Failed to create DX12 swap chain: {}", hr);
        return;
    }

    hr = swap_chain1.As(&m_swap_chain);
    if (FAILED(hr))
    {
        LOG_ERROR("Failed to query IDXGISwapChain3: {}", hr);
        return;
    }

    m_format = swap_chain_desc.Format;

    // Get back buffers
    m_back_buffers.resize(swap_chain_create_info.back_buffer_count);
    for (u32 i = 0; i < swap_chain_create_info.back_buffer_count; ++i)
    {
        hr = m_swap_chain->GetBuffer(i, IID_PPV_ARGS(&m_back_buffers[i]));
        if (FAILED(hr))
        {
            LOG_ERROR("Failed to get back buffer {}: {}", i, hr);
            return;
        }
    }

    // Create render targets for each back buffer
    // Note: We need to create render targets that wrap the back buffer resources
    // For now, we'll create placeholders that will be properly initialized in AcquireNextFrame
    render_targets.resize(swap_chain_create_info.back_buffer_count);
    for (u32 i = 0; i < render_targets.size(); i++)
    {
        render_targets[i] = Memory::Alloc<DX12RenderTarget>(
            m_context, RenderTargetCreateInfo{RenderTargetFormat::TEXTURE_FORMAT_DUMMY_COLOR,
                                              RenderTargetType::UNDEFINED, width, height});
        auto *tex = reinterpret_cast<DX12Texture *>(render_targets[i]->GetTexture());
        tex->m_resource = m_back_buffers[i];
        // Set proper metadata for the back buffer texture
        tex->m_width = width;
        tex->m_height = height;
        tex->m_format = TextureFormat::TEXTURE_FORMAT_RGBA8_UNORM;
        tex->m_type = TextureType::TEXTURE_TYPE_2D;
        tex->m_array_layer = 1;
        tex->mip_map_level = 1;
        tex->m_current_state = D3D12_RESOURCE_STATE_PRESENT;
        tex->m_state = ResourceState::RESOURCE_STATE_PRESENT;
        tex->SetAllSubresourceStates(D3D12_RESOURCE_STATE_PRESENT);
    }
#else
    LOG_ERROR("DX12 is only supported on Windows");
#endif
}

DX12SwapChain::~DX12SwapChain() noexcept
{
    for (auto *render_target : render_targets)
    {
        if (render_target != nullptr)
        {
            Memory::Free(render_target);
        }
    }
    render_targets.clear();

    // Microsoft::WRL::ComPtr will automatically release the swap chain and back buffers
}

bool DX12SwapChain::Resize(u32 new_width, u32 new_height) noexcept
{
#ifdef _WIN32
    if (new_width == 0 || new_height == 0)
    {
        return false;
    }
    if (new_width == width && new_height == height)
    {
        return true;
    }

    for (auto &back_buffer : m_back_buffers)
    {
        back_buffer.Reset();
    }

    for (auto *render_target : render_targets)
    {
        auto *dx12_rt = reinterpret_cast<DX12RenderTarget *>(render_target);
        if (!dx12_rt)
        {
            continue;
        }

        dx12_rt->m_back_buffer_resource.Reset();
        auto *tex = reinterpret_cast<DX12Texture *>(dx12_rt->GetTexture());
        if (tex)
        {
            tex->m_resource.Reset();
            tex->m_upload_buffer.Reset();
            tex->m_upload_buffer_size = 0;
        }
    }

    HRESULT hr = m_swap_chain->ResizeBuffers(m_back_buffer_count, new_width, new_height, m_format, 0);
    if (FAILED(hr))
    {
        LOG_ERROR("Failed to resize DX12 swap chain buffers: {}", hr);
        return false;
    }

    width = new_width;
    height = new_height;

    m_back_buffers.resize(m_back_buffer_count);
    for (u32 i = 0; i < m_back_buffer_count; ++i)
    {
        hr = m_swap_chain->GetBuffer(i, IID_PPV_ARGS(&m_back_buffers[i]));
        if (FAILED(hr))
        {
            LOG_ERROR("Failed to get resized back buffer {}: {}", i, hr);
            return false;
        }

        auto *dx12_rt = reinterpret_cast<DX12RenderTarget *>(render_targets[i]);
        if (!dx12_rt)
        {
            continue;
        }

        if (dx12_rt->m_rtv_handle.ptr != 0)
        {
            m_context.device->CreateRenderTargetView(m_back_buffers[i].Get(), nullptr, dx12_rt->m_rtv_handle);
        }
        else
        {
            LOG_WARN("DX12 swap chain render target {} has no RTV handle after resize", i);
        }

        dx12_rt->m_back_buffer_resource = m_back_buffers[i];

        auto *tex = reinterpret_cast<DX12Texture *>(dx12_rt->GetTexture());
        if (tex)
        {
            tex->m_resource = m_back_buffers[i];
            tex->m_width = width;
            tex->m_height = height;
            tex->m_format = TextureFormat::TEXTURE_FORMAT_RGBA8_UNORM;
            tex->m_type = TextureType::TEXTURE_TYPE_2D;
            tex->m_array_layer = 1;
            tex->mip_map_level = 1;
            tex->m_current_state = D3D12_RESOURCE_STATE_PRESENT;
            tex->m_state = ResourceState::RESOURCE_STATE_PRESENT;
            tex->SetAllSubresourceStates(D3D12_RESOURCE_STATE_PRESENT);
        }
    }

    image_index = m_swap_chain->GetCurrentBackBufferIndex();
    current_frame_index = image_index;
    return true;
#else
    (void)new_width;
    (void)new_height;
    return false;
#endif
}

} // namespace Horizon::Backend
