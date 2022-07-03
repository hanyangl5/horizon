#include <memory>

#include "HorizonApplication.h"

using namespace Horizon;

#define WINDOW_WIDTH 1920
#define WINDOW_HEIGHT 1080

void HorizonApplication::Initilize() noexcept {
    m_window = std::make_shared<Window>("horizon", WINDOW_WIDTH, WINDOW_HEIGHT);
    m_render_system = std::make_unique<RenderSystem>(
        m_window->GetWidth(), m_window->GetHeight(), m_window);
    m_input_system = std::make_unique<InputSystem>(
        m_window, m_render_system->GetMainCamera());
}

void HorizonApplication::Tick() noexcept {
    m_input_system->Tick();
    m_render_system->Tick();
}

void HorizonApplication::Destroy() noexcept {}

void HorizonApplication::Run() noexcept {
    Initilize();
    while (!m_window->ShouldClose() && !is_quit) {
        Tick();
    }
    Destroy();
}

int main(int argc, char *argv[]) {
    std::unique_ptr<HorizonApplication> application =
        std::make_unique<HorizonApplication>();
    application->Run();
    return 0;
}
