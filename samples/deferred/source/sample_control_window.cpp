#include "sample_control_window.h"

#ifndef __ANDROID__
#include <GLFW/glfw3.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl2.h>
#include <imgui.h>
#if defined(_WIN32)
#include <windows.h>
#endif
#include <GL/gl.h>

namespace
{
void ApplyEngineeringLightTheme()
{
    ImGui::StyleColorsLight();

    ImGuiStyle &style = ImGui::GetStyle();
    style.WindowRounding = 6.0f;
    style.ChildRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding = 4.0f;
    style.TabRounding = 4.0f;
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.WindowPadding = ImVec2(12.0f, 12.0f);
    style.FramePadding = ImVec2(10.0f, 6.0f);
    style.ItemSpacing = ImVec2(10.0f, 8.0f);
    style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);

    ImVec4 *colors = style.Colors;
    colors[ImGuiCol_Text] = ImVec4(0.14f, 0.18f, 0.24f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.47f, 0.53f, 0.60f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.95f, 0.96f, 0.98f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.95f, 0.96f, 0.98f, 1.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.98f, 0.99f, 1.00f, 0.98f);
    colors[ImGuiCol_Border] = ImVec4(0.73f, 0.78f, 0.84f, 1.00f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.90f, 0.93f, 0.96f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.84f, 0.89f, 0.95f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.76f, 0.84f, 0.93f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.87f, 0.91f, 0.95f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.82f, 0.88f, 0.94f, 1.00f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.89f, 0.92f, 0.96f, 1.00f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.92f, 0.94f, 0.97f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.73f, 0.79f, 0.86f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.64f, 0.73f, 0.84f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.55f, 0.66f, 0.79f, 1.00f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.22f, 0.45f, 0.73f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.31f, 0.52f, 0.79f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.22f, 0.45f, 0.73f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.82f, 0.88f, 0.95f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.73f, 0.82f, 0.93f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.63f, 0.75f, 0.89f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.82f, 0.88f, 0.95f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.74f, 0.83f, 0.94f, 1.00f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.66f, 0.77f, 0.90f, 1.00f);
    colors[ImGuiCol_Separator] = ImVec4(0.77f, 0.81f, 0.87f, 1.00f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.53f, 0.66f, 0.82f, 1.00f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.40f, 0.56f, 0.75f, 1.00f);
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.65f, 0.75f, 0.88f, 0.60f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.53f, 0.66f, 0.82f, 0.85f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.40f, 0.56f, 0.75f, 1.00f);
    colors[ImGuiCol_Tab] = ImVec4(0.86f, 0.91f, 0.96f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.76f, 0.84f, 0.93f, 1.00f);
    colors[ImGuiCol_TabSelected] = ImVec4(0.73f, 0.82f, 0.93f, 1.00f);
    colors[ImGuiCol_TabDimmed] = ImVec4(0.90f, 0.93f, 0.97f, 1.00f);
    colors[ImGuiCol_TabDimmedSelected] = ImVec4(0.82f, 0.88f, 0.95f, 1.00f);
}
} // namespace
#endif

SampleControlWindow::~SampleControlWindow()
{
    Shutdown();
}

void SampleControlWindow::Initialize(Horizon::Window *target_window, const VSyncCallback &vsync_callback,
                                     const ResizeCallback &resize_callback)
{
    m_target_window = target_window;
    m_vsync_callback = vsync_callback;
    m_resize_callback = resize_callback;

#ifndef __ANDROID__
    if (m_initialized)
    {
        return;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    m_window = glfwCreateWindow(420, 220, "Horizon Controls", nullptr, nullptr);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    if (!m_window)
    {
        return;
    }

    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(0);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ApplyEngineeringLightTheme();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui_ImplGlfw_InitForOpenGL(m_window, true);
    ImGui_ImplOpenGL2_Init();

    if (m_target_window && m_target_window->GetWindow())
    {
        int current_width = 0;
        int current_height = 0;
        glfwGetWindowSize(m_target_window->GetWindow(), &current_width, &current_height);
        if (current_width > 0 && current_height > 0)
        {
            m_pending_width = current_width;
            m_pending_height = current_height;
        }
    }

    glfwMakeContextCurrent(nullptr);
    m_initialized = true;
#else
    (void)target_window;
    (void)vsync_callback;
    (void)resize_callback;
#endif
}

void SampleControlWindow::Shutdown()
{
#ifndef __ANDROID__
    if (!m_initialized)
    {
        return;
    }

    glfwMakeContextCurrent(m_window);
    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(m_window);
    glfwMakeContextCurrent(nullptr);
    m_window = nullptr;
#endif
    m_initialized = false;
}

void SampleControlWindow::RenderFrame(bool vsync_enabled)
{
#ifndef __ANDROID__
    if (!m_initialized || !m_window)
    {
        return;
    }

    if (glfwWindowShouldClose(m_window))
    {
        Shutdown();
        return;
    }

    glfwMakeContextCurrent(m_window);

    ImGui_ImplOpenGL2_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_Always);
    if (ImGui::Begin("Sample Control", nullptr,
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse))
    {
        int current_width = 0;
        int current_height = 0;
        if (m_target_window && m_target_window->GetWindow())
        {
            glfwGetWindowSize(m_target_window->GetWindow(), &current_width, &current_height);
        }

        ImGui::Text("Window Size: %d x %d", current_width, current_height);
        ImGui::Separator();

        bool requested_vsync = vsync_enabled;
        if (ImGui::Checkbox("VSync", &requested_vsync) && m_vsync_callback)
        {
            m_vsync_callback(requested_vsync);
        }

        ImGui::InputInt("Width", &m_pending_width);
        const bool width_field_active = ImGui::IsItemActive() || ImGui::IsItemFocused();
        ImGui::InputInt("Height", &m_pending_height);
        const bool height_field_active = ImGui::IsItemActive() || ImGui::IsItemFocused();

        if (!width_field_active && !height_field_active && current_width > 0 && current_height > 0 &&
            (m_pending_width != current_width || m_pending_height != current_height))
        {
            m_pending_width = current_width;
            m_pending_height = current_height;
        }

        if (ImGui::Button("Apply Window Size") && m_resize_callback)
        {
            m_pending_width = (m_pending_width < 64) ? 64 : m_pending_width;
            m_pending_height = (m_pending_height < 64) ? 64 : m_pending_height;
            m_resize_callback(static_cast<Horizon::u32>(m_pending_width), static_cast<Horizon::u32>(m_pending_height));
        }

        const float fps = ImGui::GetIO().Framerate;
        const float frame_time_ms = fps > 0.0f ? 1000.0f / fps : 0.0f;
        ImGui::Separator();
        ImGui::Text("Frame Rate: %.1f FPS", fps);
        ImGui::Text("Frame Time: %.2f ms", frame_time_ms);
    }
    ImGui::End();

    ImGui::Render();
    int display_width = 0;
    int display_height = 0;
    glfwGetFramebufferSize(m_window, &display_width, &display_height);
    glViewport(0, 0, display_width, display_height);
    glClearColor(0.93f, 0.95f, 0.98f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(m_window);
    glfwMakeContextCurrent(nullptr);
#else
    (void)vsync_enabled;
#endif
}
