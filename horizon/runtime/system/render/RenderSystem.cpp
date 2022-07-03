#include "RenderSystem.h"

#include <iostream>
#include <runtime/core/jobsystem/ThreadPool.h>
#include <runtime/core/math/Math.h>
#include <runtime/core/path/Path.h>
#include <runtime/function/rhi/RHIInterface.h>
#include <runtime/function/rhi/ResourceBarrier.h>
#include <runtime/function/rhi/dx12/RHIDX12.h>
#include <runtime/function/rhi/vulkan/RHIVulkan.h>


namespace Horizon {

class Window;

RenderSystem::RenderSystem(u32 width, u32 height,
                           std::shared_ptr<Window> window) noexcept
    : m_window(window) {

  RenderBackend backend = RenderBackend::RENDER_BACKEND_VULKAN;

  // RenderBackend backend = RenderBackend::RENDER_BACKEND_DX12;

  InitializeRenderAPI(backend);
}

RenderSystem::~RenderSystem() noexcept {}

void RenderSystem::Tick() noexcept {
  Update();
  Render();
}

std::shared_ptr<Camera> RenderSystem::GetMainCamera() const noexcept {
  return std::shared_ptr<Camera>();
}

void RenderSystem::Update() noexcept {
  // for (size_t i = 0; i < 100; i++)
  { RunRenderTest(); }
}

void RenderSystem::Render() noexcept {}

void RenderSystem::InitializeRenderAPI(RenderBackend backend) noexcept {
  switch (backend) {
  case Horizon::RenderBackend::RENDER_BACKEND_NONE:
    break;
  case Horizon::RenderBackend::RENDER_BACKEND_VULKAN:
    m_render_api = std::make_shared<RHI::RHIVulkan>();
    break;
  case Horizon::RenderBackend::RENDER_BACKEND_DX12:
    m_render_api = std::make_shared<RHI::RHIDX12>();
    break;
  }
  m_render_api->InitializeRenderer();
  LOG_DEBUG("size of render api {}", sizeof(*m_render_api.get()));
  m_render_api->CreateSwapChain(m_window);
}
} // namespace Horizon