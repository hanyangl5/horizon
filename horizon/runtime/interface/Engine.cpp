#include "Engine.h"
#include <thread>

namespace Horizon {

Engine::Engine(const EngineConfig &config) noexcept {

    Memory::initialize();
    if (!config.offscreen)
        m_window = Memory::MakeUnique<Window>("horizon", config.width, config.height);
    m_render_system =
        Memory::MakeUnique<RenderSystem>(config.width, config.width, m_window.get(), config.render_backend, config.offscreen);
    if (!config.offscreen)
        m_input_system = Memory::MakeUnique<InputSystem>(m_window.get());
}
Engine::~Engine() noexcept { Memory::destroy(); }

void Engine::BeginNewFrame() const {

    auto rhi = m_render_system->GetRhi();
    rhi->ResetRHIResources();
    rhi->ResetFence(CommandQueueType::GRAPHICS);
    rhi->ResetFence(CommandQueueType::COMPUTE);
    rhi->ResetFence(CommandQueueType::TRANSFER);
    LOG_DEBUG("begin frame");
}

void Engine::EndFrame() const {
    // TODO: wait for gpu execution?
    auto rhi = m_render_system->GetRhi();
    rhi->WaitGpuExecution(CommandQueueType::GRAPHICS);
    rhi->WaitGpuExecution(CommandQueueType::COMPUTE);
    rhi->WaitGpuExecution(CommandQueueType::TRANSFER);
    LOG_DEBUG("end frame");
}
} // namespace Horizon
