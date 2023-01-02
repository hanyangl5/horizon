/*****************************************************************//**
 * \file   HorizonRuntime.h
 * \brief  
 * 
 * \author hylu
 * \date   November 2022
 *********************************************************************/

#pragma once

#include <runtime/core/memory/Memory.h>
#include <runtime/core/window/Window.h>

#include <runtime/function/rhi/rhi_utils.h>

#include <runtime/system/render/RenderSystem.h>

#include <runtime/interface/horizon_config.h>

namespace Horizon {

class HorizonRuntime final {
  public:
    HorizonRuntime(const InitilizationConfig &config) noexcept;
    ~HorizonRuntime() noexcept;

    HorizonRuntime(const HorizonRuntime &rhs) noexcept = delete;
    HorizonRuntime &operator=(const HorizonRuntime &rhs) noexcept = delete;
    HorizonRuntime(HorizonRuntime &&rhs) noexcept = delete;
    HorizonRuntime &operator=(HorizonRuntime &&rhs) noexcept = delete;

    void BeginNewFrame() const;
    void EndFrame() const;

  public:
    Memory::UniquePtr<Window> m_window{};
    Memory::UniquePtr<RenderSystem> m_render_system{};
};

} // namespace Horizon
