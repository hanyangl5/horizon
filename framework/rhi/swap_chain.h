#pragma once

#include <core/glfwwindow.h>

#include <core/definations.h>
#include <rhi/enums.h>
#include <rhi/render_target.h>
#include <rhi/semaphore.h>

namespace Horizon::Backend
{

struct SwapChainSemaphoreContext
{
    u32 current_frame_index{0};
    std::vector<Semaphore *> render_complete_semaphore;
    std::vector<Semaphore *> present_complete_semaphore;
    // std::vector<Semaphore *> recycled_semaphores;
    // Semaphore *swap_chain_acquire_semaphore{};
    // Semaphore *swap_chain_release_semaphore{};
};

class SwapChain
{
  public:
    SwapChain(const SwapChainCreateInfo &swap_chain_create_info, Window *window) noexcept
        : m_back_buffer_count(swap_chain_create_info.back_buffer_count)
    {
        width = window->GetWidth();
        height = window->GetHeight();
    };
    virtual ~SwapChain() noexcept = default;

    SwapChain(const SwapChain &rhs) noexcept = delete;
    SwapChain &operator=(const SwapChain &rhs) noexcept = delete;
    SwapChain(SwapChain &&rhs) noexcept = delete;
    SwapChain &operator=(SwapChain &&rhs) noexcept = delete;

    RenderTarget *GetRenderTarget() noexcept
    {
        return render_targets[current_frame_index];
    }

  public:
    std::vector<RenderTarget *> render_targets{};
    u32 m_back_buffer_count{};
    u32 current_frame_index{0};
    u32 image_index{};
    u32 width{};
    u32 height{};

  protected:
};
} // namespace Horizon::Backend
