set(EDITOR_SOURCE_DIR ${ENGINE_SOURCE_DIR}/Editor)
set(EDITOR_FILES
    ${EDITOR_SOURCE_DIR}/Public/Editor/SceneInspector.h
    ${EDITOR_SOURCE_DIR}/Public/Editor/ConsolePanel.h
    ${EDITOR_SOURCE_DIR}/Private/SceneInspector.cpp
    ${EDITOR_SOURCE_DIR}/Private/ConsolePanel.cpp
    ${EDITOR_SOURCE_DIR}/Private/InspectorWindow.cpp
    ${EDITOR_SOURCE_DIR}/Private/InspectorWindow.h
)
set(EDITOR_IMGUI_BACKENDS
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/imgui/backends/imgui_impl_win32.cpp
    ${ENGINE_THIRD_PARTY_SOURCE_DIR}/imgui/backends/imgui_impl_dx11.cpp
)

add_library(HorizonEditor STATIC EXCLUDE_FROM_ALL ${EDITOR_FILES} ${EDITOR_IMGUI_BACKENDS})
target_compile_features(HorizonEditor PUBLIC cxx_std_20)
target_include_directories(HorizonEditor PUBLIC ${EDITOR_SOURCE_DIR}/Public PRIVATE ${ENGINE_THIRD_PARTY_SOURCE_DIR}/imgui)
target_link_libraries(HorizonEditor PUBLIC ${ENGINE_RUNTIME} PRIVATE imgui d3d11 d3dcompiler dwmapi comctl32)
source_group(TREE ${EDITOR_SOURCE_DIR} FILES ${EDITOR_FILES})
source_group("ImGui backends" FILES ${EDITOR_IMGUI_BACKENDS})
set_target_properties(HorizonEditor PROPERTIES FOLDER "Horizon/Editor")
