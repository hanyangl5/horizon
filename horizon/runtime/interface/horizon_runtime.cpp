/*****************************************************************//**
 * \file   HorizonRuntime.cpp
 * \brief  
 * 
 * \author hylu
 * \date   November 2022
 *********************************************************************/

#include "horizon_runtime.h"

// standard libraries

// third party libraries

// project headers

#include <thread>

namespace Horizon {

HorizonRuntime::HorizonRuntime(const InitilizationConfig &config) noexcept {
    Memory::initialize();
    if (!config.offscreen) {
        m_window = Memory::MakeUnique<Window>("horizon", config.width, config.height);
        m_render_system = Memory::MakeUnique<RenderSystem>(config.width, config.width, m_window.get(),
                                                           config.render_backend, config.offscreen);
    } else {
        m_render_system = Memory::MakeUnique<RenderSystem>(config.width, config.width, m_window.get(),
                                                           config.render_backend, true);
    }
}

HorizonRuntime::~HorizonRuntime() noexcept {
    Memory::destroy(); 
}

void HorizonRuntime::BeginNewFrame() const {
    auto rhi = m_render_system->GetRhi();
    rhi->ResetRHIResources();
    rhi->ResetFence(CommandQueueType::GRAPHICS);
    rhi->ResetFence(CommandQueueType::COMPUTE);
    rhi->ResetFence(CommandQueueType::TRANSFER);
    LOG_DEBUG("begin frame");
}

void HorizonRuntime::EndFrame() const {
    // TODO(hylu): wait for gpu execution?
    auto rhi = m_render_system->GetRhi();
    rhi->WaitGpuExecution(CommandQueueType::GRAPHICS);
    rhi->WaitGpuExecution(CommandQueueType::COMPUTE);
    rhi->WaitGpuExecution(CommandQueueType::TRANSFER);
    LOG_DEBUG("end frame");
}

} // namespace Horizon
