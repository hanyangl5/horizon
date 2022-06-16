#pragma once

#include <memory>
#include <runtime/function/rhi/RHIUtils.h>
#include <runtime/core/window/Window.h>
#include <runtime/system/render/RenderSystem.h>
#include <runtime/system/input/InputSystem.h>
#include <runtime/interface/engine_runtime.h>

class HorizonApplication : public EngineRuntime
{
public:
	virtual void Initilize() noexcept override;
	virtual void Tick() noexcept override;
	virtual void Destroy() noexcept override;
	void Run() noexcept;
private:
	std::shared_ptr<Horizon::Window> m_window = nullptr;
	std::unique_ptr<Horizon::RenderSystem> m_render_system = nullptr;
	std::unique_ptr<Horizon::InputSystem> m_input_system;
	bool is_quit = false;
};
