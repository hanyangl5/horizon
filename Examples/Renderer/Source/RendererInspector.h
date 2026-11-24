#pragma once

#include "Scene/SceneComponents.h"
#include "Scene/World.h"
#include "imgui.h"

struct WindowHandle;
struct InspectorWindow;

class RendererInspector
{
public:
    RendererInspector(const WindowHandle& Owner, hz::World& World, const hz::TypeRegistry& Types, hz::Entity Camera);
    ~RendererInspector();

    void       Update();
    // Valid until the next Update. Driven transforms consume the edit before scene extraction.
    hz::Entity EditedPose;
    Matrix4    NewPose;

private:
    void Draw();
    void DrawList();
    void DrawComponents();
    void DrawProperty(const hz::TypeDesc& Type, const hz::PropertyDesc& Field, const void* Source, const hz::PropertyDesc* Array = nullptr,
                      uint32_t Index = 0);
    void DrawArray(const hz::TypeDesc& Type, const hz::PropertyDesc& Field, const void* Source);
    const char* Label(hz::Entity Entity, char (&Buffer)[96]) const;

    hz::World&              World;
    const hz::TypeRegistry& Types;
    const hz::Entity        Camera;
    hz::WorldQuery          Query;
    InspectorWindow*        Window;
    hz::Entity              Selection;
    const hz::TypeDesc*     Filter = nullptr;
    ImGuiTextFilter         Search;
    hz::Array<hz::Entity>   Rows;
    hz::Array<char>         StringBuffer;
    double                  RefreshAt = 0;
    uint32_t                ArrayVersion = 0;
    ImGuiID                 FailedField = 0;
};
