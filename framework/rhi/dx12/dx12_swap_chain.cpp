#include "dx12_swap_chain.h"
#include <core/glfwwindow.h>
#include <core/log.h>
#include <core/memory.h>
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include "GLFW/glfw3native.h"
#endif

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
    GLFWwindow *w = window->GetWindow();
    HWND hwnd = reinterpret_cast<HWND>(glfwGetWin32Window(w));

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

    ComPtr<IDXGISwapChain1> swap_chain1;
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
    render_targets.resize(swap_chain_create_info.back_buffer_count);
    for (u32 i = 0; i < swap_chain_create_info.back_buffer_count; ++i)
    {
        // TODO: Create render target from back buffer
        // This requires creating a texture wrapper and render target
        // For now, we'll create a placeholder
        RenderTargetCreateInfo rt_create_info{};
        rt_create_info.width = width;
        rt_create_info.height = height;
        rt_create_info.rt_format = TextureFormat::TEXTURE_FORMAT_RGBA8_UNORM;
        rt_create_info.rt_type = RenderTargetType::COLOR;
        render_targets[i] = Memory::Alloc<DX12RenderTarget>(context, rt_create_info);
    }
#else
    LOG_ERROR("DX12 is only supported on Windows");
#endif
}

DX12SwapChain::~DX12SwapChain() noexcept
{
    // ComPtr will automatically release the swap chain and back buffers
}

} // namespace Horizon::Backend
