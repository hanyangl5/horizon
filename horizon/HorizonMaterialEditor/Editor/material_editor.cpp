#include "material_editor.h"

#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include "imgui_internal.h"
#include "imgui_stdlib.h"

#include <nfd.h>

namespace Horizon::MaterialEditor {

MaterialEditor::MaterialEditor() noexcept {

    m_imgui_wrapper = std::make_unique<ImguiWrapper>();

    while (!m_imgui_wrapper->WindowShouldClose()) {
        m_imgui_wrapper->Prepare();

        MenuActions();

        for (auto &window : opened_materials) {
            MaterialEditorWindow(window);
        }
        // StartUpWindow();
        m_imgui_wrapper->RenderUI();
    }
}

MaterialEditor::~MaterialEditor() noexcept {}

void MaterialEditor::MaterialEditorWindow(HorizonMaterial *material) {
    if (material->name.empty()) {
        material->name = "material: " + std::to_string(opened_materials.size());
    }
    ImGui::Begin(material->name.c_str());
    if (ImGui::Button("save material")) {
        material->name;
    }

    if (material == nullptr) {
        //
    } else {
    }
    // ImGui::ListBox();

    ImGui::End();
}

HorizonMaterial *MaterialEditor::OpenMaterial(const std::string &path) {
    // validation

    // serilization
    return {};
}

void MaterialEditor::SaveMaterial(const std::string &path) {
    // serialize
    HorizonMaterialHeader;
    HorizonMaterialEnd;
}

std::string MaterialEditor::SelectFromExplorer() {
    nfdchar_t *outPath = NULL;
    nfdresult_t result = NFD_OpenDialog(NULL, NULL, &outPath);
    assert(result == NFD_OKAY);
    std::string path(outPath);
    free(outPath);
    return path;
}

void MaterialEditor::MenuActions() {

    if (ImGui::BeginMainMenuBar()) {

        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Material")) {
                opened_materials.push_back(new HorizonMaterial);
            }
            if (ImGui::MenuItem("Open Material")) {
                HorizonMaterial *mat = OpenMaterial(SelectFromExplorer());
                opened_materials.push_back(mat);
            }
            ImGui::EndMenu();
        }

        // if (ImGui::BeginMenu("Edit")) {
        //     if (ImGui::MenuItem("Undo", "CTRL+Z")) {
        //     }
        //     if (ImGui::MenuItem("Redo", "CTRL+Y", false, false)) {
        //     } // Disabled item
        //     ImGui::Separator();
        //     if (ImGui::MenuItem("Cut", "CTRL+X")) {
        //     }
        //     if (ImGui::MenuItem("Copy", "CTRL+C")) {
        //     }
        //     if (ImGui::MenuItem("Paste", "CTRL+V")) {
        //     }
        //     ImGui::EndMenu();
        // }

        ImGui::EndMainMenuBar();
    }
}

} // namespace Horizon::MaterialEditor