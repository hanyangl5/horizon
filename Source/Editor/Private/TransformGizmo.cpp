#include "TransformGizmo.h"

static constexpr int RingSegments = 64;

static float Dot2(float2 A, float2 B) { return A.x * B.x + A.y * B.y; }
static float Cross2(float2 A, float2 B) { return A.x * B.y - A.y * B.x; }

void TransformGizmo::SetTool(Tool Value)
{
    if (ActiveAxis < 0)
    {
        Mode = Value;
    }
}

void TransformGizmo::Cancel()
{
    ActiveAxis = -1;
    Released = Changed = false;
    Entity = {};
}

bool TransformGizmo::Press(float2 Position)
{
    if (!Entity.IsValid() || ActiveAxis >= 0)
    {
        return false;
    }
    const bool Ring = Mode == Tool::Rotate;
    float      Best = Radius * Radius * 0.01f;
    for (int I = 0; I < 3; ++I)
    {
        const float2 U = Axis[(I + 1) % 3], V = Axis[(I + 2) % 3];
        float2       A = Ring ? U : Axis[I] * 0.12f;
        for (int J = 1; J <= (Ring ? RingSegments : 1); ++J)
        {
            const float  T = J * (2.0f * PI / RingSegments);
            const float2 B = Ring ? U * cosf(T) + V * sinf(T) : Axis[I];
            const float2 Edge = B - A;
            const float  Length2 = Dot2(Edge, Edge);
            if (Length2 > (Ring ? 0.01f : 9.0f))
            {
                const float2 Offset = Position - Center - A;
                const float  Along = fminf(1, fmaxf(0, Dot2(Offset, Edge) / Length2));
                const float2 Error = Offset - Edge * Along;
                const float  Distance2 = Dot2(Error, Error);
                if (Distance2 < Best)
                {
                    Best = Distance2;
                    ActiveAxis = I;
                    RingU = U;
                    RingV = V;
                    DragDirection = Edge / Length2;
                    DragScale = Ring ? 2.0f * PI / RingSegments : 0.88f;
                }
            }
            A = B;
        }
    }
    if (ActiveAxis < 0)
    {
        return false;
    }
    Pivot = Center;
    PressPosition = Position;
    const float2 P = Position - Pivot;
    Angle = atan2f(Cross2(RingU, P), Cross2(P, RingV));
    Value = 0;
    Started = true;
    return true;
}

void TransformGizmo::Move(float2 Position)
{
    if (!Dragging())
    {
        return;
    }
    const float Previous = Value;
    if (Mode == Tool::Rotate && fabsf(Cross2(RingU, RingV)) > Radius * Radius * 0.12f)
    {
        const float2 P = Position - Pivot;
        const float  Next = atan2f(Cross2(RingU, P), Cross2(P, RingV));
        Value += remainderf(Next - Angle, 2.0f * PI);
        Angle = Next;
    }
    else
    {
        Value = Dot2(Position - PressPosition, DragDirection) * DragScale;
    }
    Changed |= Value != Previous;
}

void TransformGizmo::Release(float2 Position)
{
    if (Dragging())
    {
        Move(Position);
        Released = true;
    }
}

bool TransformGizmo::Update(hz::World& World, hz::Entity Selection, hz::Entity Camera, float2 ViewSize, float Dpi, Matrix4& Edited)
{
    if (!Selection.IsValid() || ViewSize.x <= 0 || ViewSize.y <= 0)
    {
        Cancel();
        return false;
    }
    if (Entity != Selection)
    {
        Cancel();
    }

    bool Applied = false;
    if (ActiveAxis >= 0 && Changed)
    {
        Matrix4            Matrix = Local;
        hz::LocalTransform Trs = Transform;
        const Vector3      Direction = LocalDirection[ActiveAxis];
        if (Mode == Tool::Translate)
        {
            Trs.translation = Local.getTranslation() + Direction * (Value * LocalDistance[ActiveAxis]);
            Matrix.setTranslation(Trs.translation);
        }
        else if (Mode == Tool::Rotate)
        {
            const Quat Rotation = Quat::rotation(Value, Direction);
            Matrix.setUpper3x3(Matrix3(Rotation) * Local.getUpper3x3());
            Trs.rotation = normalize(Rotation * Transform.rotation);
        }
        else
        {
            const float Factor = fmaxf(0.01f, 1.0f + Value);
            Matrix.setCol(ActiveAxis, Local.getCol(ActiveAxis) * Factor);
            Trs.scale.setElem(ActiveAxis, Transform.scale.getElem(ActiveAxis) * Factor);
        }
        Applied = MatrixMode ? World.SetComponent(Selection, hz::SceneTypes::localMatrix, &Matrix)
                             : World.SetComponent(Selection, hz::SceneTypes::localTransform, &Trs);
        Changed = false;
    }
    if (Released)
    {
        ActiveAxis = -1;
        Released = false;
    }

    World.UpdateTransforms();
    const hz::WorldTransform* Pose = World.GetWorldTransform(Selection);
    const hz::WorldTransform* Eye = World.GetWorldTransform(Camera);
    const hz::Camera*         Lens = static_cast<const hz::Camera*>(World.GetComponent(Camera, hz::SceneTypes::camera));
    if (!Pose || !Eye || !Lens)
    {
        Cancel();
        return false;
    }
    if (Applied)
    {
        Edited = Pose->current;
    }

    const float   Aspect = ViewSize.y / ViewSize.x;
    const float   TanHalf = tanf(Lens->verticalFov * 0.5f);
    const Matrix4 VP =
        Matrix4::perspectiveRH(2.0f * atanf(TanHalf / Aspect), Aspect, Lens->nearPlane, Lens->farPlane) * inverse(Eye->current);
    const Vector4 Clip = VP * Pose->current.getCol3();
    const float   W = Clip.getW();
    if (W <= Lens->nearPlane || W >= Lens->farPlane)
    {
        Cancel();
        return Applied;
    }
    Size = ViewSize;
    Radius = 78.0f * Dpi;
    const float2 Ndc((float)Clip.getX() / W, (float)Clip.getY() / W);
    Center = float2((1 + Ndc.x) * Size.x * 0.5f, (1 - Ndc.y) * Size.y * 0.5f);
    const float               WorldRadius = 2 * W * TanHalf * Radius / Size.y;
    const hz::Entity          Parent = World.GetParent(Selection);
    const hz::WorldTransform* ParentPose = Parent.IsValid() ? World.GetWorldTransform(Parent) : nullptr;
    if (ActiveAxis < 0)
    {
        const Matrix4*            M = static_cast<const Matrix4*>(World.GetComponent(Selection, hz::SceneTypes::localMatrix));
        const hz::LocalTransform* T = static_cast<const hz::LocalTransform*>(World.GetComponent(Selection, hz::SceneTypes::localTransform));
        MatrixMode = M != nullptr;
        Transform = T ? *T : hz::LocalTransform{};
        Local = M ? *M : Matrix4(Transform.rotation, Transform.translation) * Matrix4::scale(Transform.scale);
    }
    for (int I = 0; I < 3; ++I)
    {
        const Vector3 Basis = Matrix3::identity().getCol(I);
        if (ActiveAxis < 0)
        {
            const Vector3 Column = Local.getCol(I).getXYZ();
            LocalDirection[I] = !MatrixMode ? rotate(Transform.rotation, Basis) : (lengthSqr(Column) > 1e-12f ? normalize(Column) : Basis);
        }
        const Vector3 Dir = ParentPose ? ParentPose->current.getUpper3x3() * LocalDirection[I] : LocalDirection[I];
        const float   Magnitude = length(Dir);
        const float   Distance = Magnitude > 1e-6f ? WorldRadius / Magnitude : 0;
        if (ActiveAxis < 0)
        {
            LocalDistance[I] = Distance;
        }
        const Vector4 D = VP * Vector4(Dir * Distance, 0);
        Axis[I] = float2(((float)D.getX() - Ndc.x * (float)D.getW()) * Size.x / (2 * W),
                         (Ndc.y * (float)D.getW() - (float)D.getY()) * Size.y / (2 * W));
    }
    Entity = Selection;
    return Applied;
}

void TransformGizmo::Draw(hz::RenderContext& Context, hz::CommandList& Cmd, const hz::GPUTexture& Target)
{
    if (!Entity.IsValid())
    {
        return;
    }
    if (Target.GetFormat() != Format || !Pipeline.IsValid())
    {
        Format = Target.GetFormat();
        Pipeline = Context.CreateGraphicsPipeline({ .shaderDesc = { .stages = { { .stage = SHADER_STAGE_VERT, .pEntryPoint = "VSMain" },
                                                                                { .stage = SHADER_STAGE_FRAG, .pEntryPoint = "PSMain" } },
                                                                    .sourceDirectory = DirectoryMiddleware0,
                                                                    .pFileName = "TransformGizmo.slang" },
                                                    .colorTargets = { { .Format = Format } },
                                                    .Name = "Editor.TransformGizmo" });
        ASSERT(Pipeline.IsValid());
    }
    Cmd.BeginRendering(
        { .colorAttachments = { { .pTexture = &Target, .loadAction = LOAD_ACTION_LOAD, .storeAction = STORE_ACTION_STORE } } });
    Cmd.SetViewport(0, 0, (float)Target.GetWidth(), (float)Target.GetHeight());
    Cmd.SetScissor(0, 0, Target.GetWidth(), Target.GetHeight());
    Cmd.SetPipeline(Pipeline);
    for (int I = 0; I < 3; ++I)
    {
        const struct
        {
            float2   Origin, ViewSize, U, V;
            uint32_t ToolIndex, AxisIndex;
            float    Thickness;
            uint32_t OutputMode, EncodeSRGB;
        } Constants = { Center,
                        Size,
                        Mode == Tool::Rotate ? Axis[(I + 1) % 3] : Axis[I],
                        Axis[(I + 2) % 3],
                        (uint32_t)Mode,
                        (uint32_t)(I == ActiveAxis ? 3 : I),
                        Radius / 52.0f,
                        (uint32_t)Context.GetOutputMode(),
                        Format != hz::Format::R8G8B8A8_SRGB && Format != hz::Format::B8G8R8A8_SRGB };
        if (Mode == Tool::Rotate || Dot2(Constants.U, Constants.U) > 12.0f)
        {
            Cmd.SetRootConstants(Constants);
            Cmd.Draw(Mode == Tool::Rotate ? 6 * RingSegments : (Mode == Tool::Scale ? 12 : 9));
        }
    }
    Cmd.EndRendering();
}
