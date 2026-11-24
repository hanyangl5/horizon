#pragma once

#include "Core/IMath.h"
#include "Core/IUniquePtr.h"
#include "Scene/World.h"

struct WindowHandle;

class SceneInspector
{
public:
    SceneInspector(const WindowHandle& Owner, hz::World& World, const hz::TypeRegistry& Types, hz::Entity Camera);
    ~SceneInspector();

    void       Update();
    // Valid until the next Update. Driven transforms consume the edit before scene extraction.
    hz::Entity EditedPose;
    Matrix4    NewPose;

private:
    struct Panel;
    hz::UniquePtr<Panel> UI;
};
