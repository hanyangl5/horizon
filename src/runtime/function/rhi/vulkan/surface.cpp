#include "Surface.h"
#include <runtime/function/rhi/RenderContext.h>
#include <runtime/function/window/Window.h>

#include <runtime/core/log/Log.h>

namespace Horizon {

Surface::Surface(std::shared_ptr<Instance> instance, std::shared_ptr<Window> window)
    : m_instance(instance), m_window(window) {
    createSurface();
}

Surface::~Surface() { vkDestroySurfaceKHR(m_instance->Get(), m_surface, nullptr); }

VkSurfaceKHR Surface::Get() const noexcept { return m_surface; }

void Surface::createSurface() {
    CHECK_VK_RESULT(glfwCreateWindowSurface(m_instance->Get(), m_window->getWindow(), nullptr, &m_surface));
}
} // namespace Horizon