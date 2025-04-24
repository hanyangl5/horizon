#include "hub.h"
#include "../imgui/imgui_internal.h"

void ShowDefaultPanel(const char* PanelName, ImGuiWindowFlags window_flags);
void ShowEngineDevPanel(const char* PanelName, ImGuiWindowFlags window_flags);

void RenderHubUI()
{
    ImGuiWindowFlags window_flags = 0;
    // if (no_titlebar)
    // window_flags |= ImGuiWindowFlags_NoTitleBar;
    window_flags |= ImGuiWindowFlags_NoScrollbar;
    //    window_flags |= ImGuiWindowFlags_MenuBar;
    window_flags |= ImGuiWindowFlags_NoMove;
    window_flags |= ImGuiWindowFlags_NoResize;
    window_flags |= ImGuiWindowFlags_NoCollapse;
    //     window_flags |= ImGuiWindowFlags_NoNav;
    window_flags |= ImGuiWindowFlags_NoBackground;

    //     window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
    //     window_flags |= ImGuiWindowFlags_NoDocking;
    //     window_flags |= ImGuiWindowFlags_UnsavedDocument;
    //     p_open = NULL; // Don't pass our bool* to Begin

    // fullscreen dock space windows
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    int                  borderSize = 0;
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + borderSize, viewport->WorkPos.y + borderSize));
    ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x - 2 * borderSize, viewport->WorkSize.y - 2 * borderSize));
    ImGui::Begin("DockSpace Window", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

    ImGuiID dockspace_id = ImGui::GetID("GlobalDockSpace");
    ImGui::DockSpace(dockspace_id);

    {
        const char* Panel1Name = "Projects";
        const char* Panel2Name = "EngineDev";
        ShowDefaultPanel(Panel1Name, window_flags);
        ShowEngineDevPanel(Panel2Name, window_flags);

        static bool initialized = false;
        if (!initialized)
        {
            initialized = true;

            ImGui::DockBuilderRemoveNode(dockspace_id);                            // �����ǰ DockSpace �Ĳ���
            ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace); // �����µ� DockSpace �ڵ�
            ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetWindowSize());

            ImGui::DockBuilderDockWindow(Panel1Name, dockspace_id);
            ImGui::DockBuilderDockWindow(Panel2Name, dockspace_id);

            ImGui::DockBuilderFinish(dockspace_id);
        }
    }

    ImGui::End();
}
void ShowDefaultPanel(const char* PanelName, ImGuiWindowFlags window_flags)
{
    ImGui::Begin(PanelName, nullptr, window_flags); // Pass a pointer to our bool variable (the window will have
                                                           // a closing
                                                           // button that will clear the bool when clicked)
    const char* path = "xxx";

    if (ImGui::Button("Engine Path"))
    {
    }
    ImGui::SameLine();
    ImGui::Text(": %s", path); // todo: input text

    if (ImGui::Button("New Project"))
    {
    }
    ImGui::End();
}

void ShowEngineDevPanel(const char* PanelName, ImGuiWindowFlags window_flags)
{
    ImGui::Begin(PanelName, nullptr, window_flags); // Pass a pointer to our bool variable (the window will have
                                                           // a closing
                                                           // button that will clear the bool when clicked)

    if (ImGui::Button("Locate Engine Source Code"))
    {
        // Code to locate engine source code
    }

    if (ImGui::Button("Switch Engine Branch"))
    {
        // Code to switch engine branch
    }

    if (ImGui::Button("Rebuild Engine"))
    {
        // Code to rebuild engine
    }

    ImGui::End();
}