#include "Engine.h"
#include <thread>
namespace Horizon {

Engine::Engine(const EngineConfig &config) noexcept {
    if (!config.offscreen)
    m_window = std::make_unique<Window>("horizon", config.width, config.height);
    m_render_system = std::make_unique<RenderSystem>(config.width, config.width, m_window.get(), config.render_backend, config.offscreen);
    if (!config.offscreen)
    m_input_system = std::make_unique<InputSystem>(m_window.get());

}

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
