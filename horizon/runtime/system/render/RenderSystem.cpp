#include "RenderSystem.h"

#include <iostream>
#include <runtime/core/math/math.h>
#include <runtime/core/path/Path.h>
#include <runtime/function/rhi/rhi.h>
#include <runtime/function/rhi/resource_barrier.h>
#include <runtime/function/rhi/vulkan/rhi_vulkan.h>

namespace Horizon {

class Window;

RenderSystem::RenderSystem(Window *window, RenderBackend backend, bool offscreen) noexcept
    : m_window(window) {
    switch (backend) {
    case Horizon::RenderBackend::RENDER_BACKEND_NONE:
        break;
    case Horizon::RenderBackend::RENDER_BACKEND_VULKAN:
        m_rhi = Memory::MakeUnique<Backend::RHIVulkan>(offscreen);
        break;
    case Horizon::RenderBackend::RENDER_BACKEND_DX12:
        // m_rhi = Memory::MakeUnique2<Backend::RHIDX12>();
        break;
    }
    m_rhi->InitializeRenderer();

    LOG_DEBUG("size of render api {}", sizeof(*m_rhi));

    m_rhi->SetWindow(m_window);

    auto allocator = Memory::GetGlobalAllocator();

    m_resource_manager = Memory::MakeUnique<ResourceManager>(m_rhi.get(), allocator);

    m_scene_manager = Memory::MakeUnique<SceneManager>(m_resource_manager.get(), allocator);
}

RenderSystem::~RenderSystem() noexcept {}

} // namespace Horizon