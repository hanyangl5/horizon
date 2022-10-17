#include <memory>
#include <string>
#include <vector>

#include "imgui_wrapper.h"
#include "../Material/material.h"


namespace Horizon::MaterialEditor {

class MaterialEditor {
  public:
    MaterialEditor() noexcept;
    ~MaterialEditor() noexcept;

    void MaterialEditorWindow(HorizonMaterial *material);
    HorizonMaterial *MaterialEditor::OpenMaterial(const std::string &path);
    void SaveMaterial(const std::string& path);
    std::string SelectFromExplorer();

    void MenuActions();
  public:
    std::unique_ptr<ImguiWrapper> m_imgui_wrapper{};
    std::vector<HorizonMaterial *> opened_materials;
    
    // Our state
    bool show_demo_window = true;
    bool show_another_window = false;
};
} // namespace Horizon::MaterialEditor