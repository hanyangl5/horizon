#pragma once

#include <runtime/core/window/Window.h>

#include <runtime/core/utils/Definations.h>
#include <runtime/function/rhi/rhi_utils.h>
#include <runtime/function/rhi/render_target.h>
#include <runtime/function/rhi/Semaphore.h>

namespace Horizon::Backend {

struct SwapChainSemaphoreContext {
    // Container::Array<Semaphore *> recycled_semaphores;
    // Semaphore *swap_chain_acquire_semaphore{};
    // Semaphore *swap_chain_release_semaphore{};
    Container::Array<Semaphore*> recycled_semaphores;
    Semaphore* swap_chain_acquire_semaphore{};
    Semaphore* swap_chain_release_semaphore{};
};

class SwapChain {
  public:
    SwapChain(const SwapChainCreateInfo &swap_chain_create_info, Window *window) noexcept
        : m_back_buffer_count(swap_chain_create_info.back_buffer_count) {
        width = window->GetWidth();
        height = window->GetHeight();
    };
    virtual ~SwapChain() noexcept = default;

    SwapChain(const SwapChain &rhs) noexcept = delete;
    SwapChain &operator=(const SwapChain &rhs) noexcept = delete;
    SwapChain(SwapChain &&rhs) noexcept = delete;
    SwapChain &operator=(SwapChain &&rhs) noexcept = delete;

    virtual void AcquireNextFrame(SwapChainSemaphoreContext *recycled_sempahores) noexcept = 0;

    RenderTarget *GetRenderTarget() noexcept { return render_targets[current_frame_index]; }

  public:
    Container::Array<RenderTarget *> render_targets{};
    u32 m_back_buffer_count{};
    u32 current_frame_index{0};
    u32 image_index{};
    u32 width{};
    u32 height{};

  protected:
};
} // namespace Horizon::Backend
