#pragma once

#include "Graphics/RenderContext.h"
#include "Scene/SceneComponents.h"
#include "Scene/World.h"

class TransformGizmo
{
public:
    enum class Tool : uint32_t
    {
        Translate,
        Rotate,
        Scale
    };

    void SetTool(Tool Value);
    bool Press(float2 Position);
    void Move(float2 Position);
    void Release(float2 Position);
    void Cancel();
    bool Dragging() const { return ActiveAxis >= 0 && !Released; }

    bool Update(hz::World& World, hz::Entity Selection, hz::Entity Camera, float2 ViewSize, float Dpi, Matrix4& Edited);
    void Draw(hz::RenderContext& Context, hz::CommandList& Cmd, const hz::GPUTexture& Target);
    bool Started = false;

private:
    Tool               Mode = Tool::Translate;
    hz::Entity         Entity;
    float2             Size, Center, Axis[3];
    float              Radius = 0;
    Vector3            LocalDirection[3];
    float              LocalDistance[3] = {};
    Matrix4            Local;
    hz::LocalTransform Transform;
    bool               MatrixMode = false;

    int             ActiveAxis = -1;
    bool            Released = false, Changed = false;
    float2          PressPosition, DragDirection, RingU, RingV, Pivot;
    float           DragScale = 0, Angle = 0, Value = 0;
    hz::GPUPipeline Pipeline;
    hz::Format      Format = hz::Format::UNDEFINED;
};
