#include "Engine.h"
#include <thread>
namespace Horizon {

Engine::Engine(const EngineConfig &config) noexcept {
    m_window = std::make_unique<Window>("horizon", config.width, config.height);
    m_render_system = std::make_unique<RenderSystem>(
        config.width, config.width, m_window.get(), config.render_backend);

    m_input_system = std::make_unique<InputSystem>(
        m_window.get(), m_render_system->GetMainCamera());

    tp = std::make_unique<BS::thread_pool>(std::thread::hardware_concurrency() -
                                           1);
}

void Engine::BeginNewFrame() const noexcept {
    m_render_system->GetRhi()->ResetCommandResources();
}

void Engine::EndFrame() const noexcept {
    // TODO: wait for gpu execution?
    auto rhi = m_render_system->GetRhi();
    rhi->WaitGpuExecution(CommandQueueType::GRAPHICS);
    rhi->WaitGpuExecution(CommandQueueType::COMPUTE);
    rhi->WaitGpuExecution(CommandQueueType::TRANSFER);
}
} // namespace Horizon
