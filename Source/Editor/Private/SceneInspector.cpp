#include "Editor/SceneInspector.h"
#include "Scene/SceneComponents.h"
#include "InspectorWindow.h"
#include "TransformGizmo.h"
#include "Editor/ConsolePanel.h"
#include "imgui_internal.h"
#include <stdio.h>

struct SceneInspector::Panel
{
    Panel(SceneInspector& Inspector, const WindowHandle& Owner, hz::World& World, const hz::TypeRegistry& Types, hz::Entity Camera):
        Inspector(Inspector), World(World), Types(Types), Camera(Camera), Query(World, {}), Window(CreateInspectorWindow(Owner, Gizmo, Inspector.Picking))
    {
    }

    void        Draw();
    void        DrawList();
    void DrawComponents();
    void DrawProperty(const hz::TypeDesc& Type, const hz::PropertyDesc& Field, const void* Source, const hz::PropertyDesc* Array = nullptr,
                      uint32_t Index = 0);
    void DrawArray(const hz::TypeDesc& Type, const hz::PropertyDesc& Field, const void* Source);
    const char* Label(hz::Entity Entity, char (&Buffer)[96]) const;

    SceneInspector&         Inspector;
    hz::World&              World;
    const hz::TypeRegistry& Types;
    const hz::Entity        Camera;
    hz::WorldQuery          Query;
    TransformGizmo          Gizmo;
    InspectorWindow*        Window;
    void Select(hz::Entity Entity)
    {
        ++Serial;
        Selection = Entity;
        FailedField = 0;
    }

    uint64_t                Serial = 0;
    hz::Entity              Selection;
    const hz::TypeDesc*     Filter = nullptr;
    ConsolePanel            Console;
    ImGuiTextFilter         Search;
    hz::Array<hz::Entity>   Rows;
    hz::Array<char>         StringBuffer;
    double                  RefreshAt = 0;
    uint32_t                ArrayVersion = 0;
    ImGuiID                 FailedField = 0;
};

SceneInspector::SceneInspector(const WindowHandle& Owner, hz::World& World, const hz::TypeRegistry& Types, hz::Entity Camera,
                               hz::RenderContext& Context, const ViewportGeometry& Geometry):
    Picking(Context, Geometry), UI(hz::MakeUnique<Panel>(*this, Owner, World, Types, Camera))
{
    if (!UI->Window)
    {
        LOGF(Warning, "Scene Inspector window initialization failed");
    }
}

SceneInspector::~SceneInspector() { DestroyInspectorWindow(UI->Window); }

void SceneInspector::OnPresented(hz::SubmitHandle Submission)
{
    Picking.OnSubmitted(Submission);
    OnViewportPresented(UI->Window);
}

void SceneInspector::RecordOverlay(hz::RenderContext& Context, hz::CommandList& Commands, const hz::GPUTexture& Backbuffer,
                                   const hz::GPUBuffer& Frame, const hz::SceneView& View, const hz::GpuDrawList& Draws,
                                   const hz::GPUSampler& Sampler, bool Transparent, float JitterX, float JitterY)
{
    if (!IsInspectorVisible(UI->Window))
    {
        return;
    }
    const hz::RenderPrimitiveID Selected = UI->Selection.IsValid() ? View.Scene.FindPrimitive(UI->Selection) : hz::RenderPrimitiveID{};
    if (Picking.HasWork(Selected))
    {
        Picking.Record(Commands, Frame, View, Draws, Sampler, Transparent, Selected, Backbuffer, JitterX, JitterY);
    }
    UI->Gizmo.Draw(Context, Commands, Backbuffer);
}

uint64_t SceneInspector::TakeViewportClick(uint32_t Width, uint32_t Height, uint32_t& X, uint32_t& Y)
{
    return ConsumeViewportClick(UI->Window, Width, Height, X, Y) ? ++UI->Serial : 0;
}

void SceneInspector::ApplyPick(uint64_t Serial, const hz::RenderScene& Scene, hz::RenderInstanceID Id)
{
    if (Serial != UI->Serial)
    {
        return;
    }
    hz::Entity                Entity;
    const hz::RenderInstance* Instance = Scene.GetInstance(Id);
    if (Instance && Instance->entityGuid.IsValid())
    {
        Entity = UI->World.GetEntity(Instance->entityGuid);
        if (Scene.FindPrimitive(Entity) != Instance->primitive)
        {
            Entity = {};
        }
    }
    else if (Instance)
    {
        struct Search
        {
            const hz::RenderScene& Scene;
            hz::RenderPrimitiveID  Primitive;
            hz::Entity             Found;
        } Match = { Scene, Instance->primitive, {} };
        UI->Query.Each(
            [](hz::Entity Candidate, hz::Span<const void*>, void* User)
            {
                Search& Match = *static_cast<Search*>(User);
                if (Match.Scene.FindPrimitive(Candidate) == Match.Primitive)
                {
                    Match.Found = Candidate;
                    return false;
                }
                return true;
            },
            &Match);
        Entity = Match.Found;
    }
    UI->Select(Entity.IsValid() && UI->World.IsAlive(Entity) ? Entity : hz::Entity{});
}

void SceneInspector::Update(const hz::RenderScene& Scene)
{
    EditedPose = {};
    if (!IsInspectorVisible(UI->Window))
    {
        return;
    }
    if (UI->Gizmo.Started)
    {
        ++UI->Serial;
        UI->Gizmo.Started = false;
    }
    Picking.Update(*this, Scene);
    if (UI->Selection.IsValid() && !UI->World.IsAlive(UI->Selection))
    {
        UI->Select({});
        UI->RefreshAt = 0;
    }
    DrawInspectorWindow(UI->Window, [](void* User) { static_cast<Panel*>(User)->Draw(); }, UI.Get());
    float Dpi = 1;
    const float2 Size = GetViewportSize(UI->Window, Dpi);
    if (UI->Gizmo.Update(UI->World, UI->Selection, UI->Camera, Size, Dpi, NewPose))
    {
        EditedPose = UI->Selection;
    }
}

const char* SceneInspector::Panel::Label(hz::Entity Entity, char (&Buffer)[96]) const
{
    const hz::Name* Name = (const hz::Name*)World.GetComponent(Entity, hz::SceneTypes::name);
    if (Name && Name->value[0])
    {
        return Name->value;
    }
    const char*      Kind = "Entity";
    const hz::Light* Light = (const hz::Light*)World.GetComponent(Entity, hz::SceneTypes::light);
    if (Light)
    {
        Kind = "Light";
        const hz::PropertyDesc* Field = Types.GetType(hz::SceneTypes::light)->GetProperty("type");
        if (Field)
        {
            for (const hz::EnumValue& Value : Field->enumValues)
            {
                if (Value.value == (int32_t)Light->Type)
                {
                    Kind = Value.Name;
                }
            }
        }
    }
    else if (World.GetComponent(Entity, hz::SceneTypes::camera))
    {
        Kind = "Camera";
    }
    else if (World.GetComponent(Entity, hz::SceneTypes::meshRenderer))
    {
        Kind = "Mesh";
    }
    const hz::Guid ID = World.GetEntityGuid(Entity);
    snprintf(Buffer, sizeof(Buffer), "%s%s %08X", Kind, Light ? " light" : "",
             ID.IsValid() ? uint32_t(ID.high >> 32) : ImHashData(&Entity, sizeof(Entity)));
    return Buffer;
}

void SceneInspector::Panel::DrawList()
{
    if (Search.DrawWithHint("Search", "Name or short ID"))
    {
        RefreshAt = 0;
    }
    if (ImGui::BeginCombo("Component", Filter ? Filter->Name : "All components"))
    {
        if (ImGui::Selectable("All components", !Filter))
        {
            Filter = nullptr;
            RefreshAt = 0;
        }
        for (uint32_t I = 0; I < Types.Size(); ++I)
        {
            const hz::TypeDesc* Type = Types.GetTypeAt(I);
            if (ImGui::Selectable(Type->Name, Filter == Type))
            {
                Filter = Type;
                RefreshAt = 0;
            }
        }
        ImGui::EndCombo();
    }
    if (ImGui::GetTime() >= RefreshAt)
    {
        Rows.Clear();
        Query.Each(
            [](hz::Entity Entity, hz::Span<const void*>, void* User)
            {
                Panel& Inspector = *static_cast<Panel*>(User);
                char   Buffer[96];
                if ((!Inspector.Filter || Inspector.World.GetComponent(Entity, Inspector.Filter->id)) &&
                    (!Inspector.Search.IsActive() || Inspector.Search.PassFilter(Inspector.Label(Entity, Buffer))))
                {
                    Inspector.Rows.PushBack(Entity);
                }
                return true;
            },
            this);
        RefreshAt = ImGui::GetTime() + 0.5;
    }
    ImGui::Text("%u entities", Rows.Size());
    if (ImGui::BeginChild("Entities", ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 9),
                          ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeY))
    {
        ImGuiListClipper Clipper;
        Clipper.Begin((int)Rows.Size());
        while (Clipper.Step())
        {
            for (int I = Clipper.DisplayStart; I < Clipper.DisplayEnd; ++I)
            {
                const hz::Entity Entity = Rows[I];
                if (!World.IsAlive(Entity))
                {
                    ImGui::Dummy(ImVec2(0, ImGui::GetTextLineHeight()));
                    continue;
                }
                char Buffer[96];
                ImGui::PushID((int)ImHashData(&Entity, sizeof(Entity)));
                if (ImGui::Selectable(Label(Entity, Buffer), Entity == Selection))
                {
                    Select(Entity);
                }
                ImGui::PopID();
            }
        }
    }
    ImGui::EndChild();
}

void SceneInspector::Panel::Draw()
{
    Console.Draw();
    DrawList();
    if (!Selection.IsValid())
    {
        ImGui::TextDisabled("Select an entity to inspect its components.");
        return;
    }
    ImGui::PushID((int)ImHashData(&Selection, sizeof(Selection)));
    ImGui::TextDisabled("Viewport: W Move   E Rotate   R Scale   |   RMB + WASD Fly");
    DrawComponents();
    ImGui::PopID();
}

void SceneInspector::Panel::DrawComponents()
{
    char Buffer[96];
    ImGui::SeparatorText(Label(Selection, Buffer));
    const hz::Entity Parent = World.GetParent(Selection);
    ImGui::Text("Parent: %s", Parent.IsValid() ? Label(Parent, Buffer) : "(root)");
    for (uint32_t I = 0; I < Types.Size(); ++I)
    {
        const hz::TypeDesc& Type = *Types.GetTypeAt(I);
        if (!World.GetComponent(Selection, Type.id) || !ImGui::CollapsingHeader(Type.Name, ImGuiTreeNodeFlags_DefaultOpen))
        {
            continue;
        }
        ImGui::PushID(Type.Name);
        for (const hz::PropertyDesc& Field : Type.properties)
        {
            const uint8_t* Component = (const uint8_t*)World.GetComponent(Selection, Type.id);
            ImGui::BeginDisabled((Field.Flags & hz::PROPERTY_READ_ONLY) != 0);
            if (Field.kind == hz::PropertyKind::Array)
            {
                DrawArray(Type, Field, Component + Field.offset);
            }
            else
            {
                DrawProperty(Type, Field, Component + Field.offset);
            }
            ImGui::EndDisabled();
        }
        ImGui::PopID();
    }
    if (ImGui::CollapsingHeader("World transform (read only)"))
    {
        World.UpdateTransforms();
        const hz::WorldTransform* Transform = World.GetWorldTransform(Selection);
        if (Transform)
        {
            for (int I = 0; I < 4; ++I)
            {
                const Vector4 Row = Transform->current.getRow(I);
                ImGui::Text("% .3f    % .3f    % .3f    % .3f", (float)Row[0], (float)Row[1], (float)Row[2], (float)Row[3]);
            }
        }
    }
}

static bool EditMatrix(Matrix4& Pose, bool FpsCamera)
{
    bool Changed = ImGui::DragFloat3("Position", (float*)&Pose[3], 0.05f);
    if (FpsCamera)
    {
        Vector3 Forward = -Pose.getCol2().getXYZ();
        if (ImGui::DragFloat3("Forward", (float*)&Forward, 0.01f) && lengthSqr(Forward) > 0.0001f)
        {
            const Vector3 Z = -normalize(Forward);
            if (fabsf(Z.getY()) < 0.9998f)
            {
                const Vector3 X = normalize(cross(Vector3(0, 1, 0), Z));
                Pose = Matrix4(Vector4(X, 0), Vector4(cross(Z, X), 0), Vector4(Z, 0), Pose.getCol3());
                Changed = true;
            }
        }
    }
    else if (ImGui::TreeNode("Matrix columns"))
    {
        for (int I = 0; I < 4; ++I)
        {
            const char* Labels[] = { "X", "Y", "Z", "W" };
            Changed |= ImGui::DragFloat4(Labels[I], (float*)&Pose[I], 0.01f);
        }
        ImGui::TreePop();
    }
    return Changed;
}

void SceneInspector::Panel::DrawProperty(const hz::TypeDesc& Type, const hz::PropertyDesc& Field, const void* Source,
                                         const hz::PropertyDesc* Array, uint32_t Index)
{
    using Kind = hz::PropertyKind;
    alignas(16) uint8_t Bytes[sizeof(Matrix4)] = {};
    void*               Value = Bytes;
    uint32_t            Size = Field.Size;
    if (Field.kind == Kind::String)
    {
        StringBuffer.Resize(Size);
        Value = StringBuffer.GetData();
    }
    memcpy(Value, Source, Size);
    bool          Changed = false;
    const char*   Name = Field.Name;
    const ImGuiID ID = ImGui::GetID(Name);
    const float   Step = Field.step > 0 ? (float)Field.step : 0.05f;
    float*        Floats = (float*)Value;
    Matrix4       Pose;
    switch (Field.kind)
    {
    case Kind::Boolean:
        Changed = ImGui::Checkbox(Name, (bool*)Value);
        break;
    case Kind::UnsignedInteger:
    case Kind::SignedInteger:
    {
        const ImGuiDataType SignedType = Size == 1   ? ImGuiDataType_S8
                                         : Size == 2 ? ImGuiDataType_S16
                                         : Size == 4 ? ImGuiDataType_S32
                                                     : ImGuiDataType_S64;
        Changed = ImGui::InputScalar(Name, SignedType + (Field.kind == Kind::UnsignedInteger), Value, nullptr, nullptr, nullptr,
                                     ImGuiInputTextFlags_EnterReturnsTrue);
        break;
    }
    case Kind::Float:
        if (Size == sizeof(double))
        {
            const bool Limited = Field.minimum < Field.maximum;
            Changed = ImGui::DragScalar(Name, ImGuiDataType_Double, Value, Step, Limited ? &Field.minimum : nullptr,
                                        Limited ? &Field.maximum : nullptr, nullptr, ImGuiSliderFlags_AlwaysClamp);
        }
        else
        {
            Changed = ImGui::DragFloat(Name, Floats, Step, (float)Field.minimum, (float)Field.maximum, "%.3f",
                                       Field.minimum < Field.maximum ? ImGuiSliderFlags_AlwaysClamp : ImGuiSliderFlags_None);
        }
        break;
    case Kind::Vector2:
        Changed = ImGui::DragFloat2(Name, Floats, Step);
        break;
    case Kind::Vector3:
        Changed = ImGui::DragFloat3(Name, Floats, Step);
        break;
    case Kind::Vector4:
    case Kind::Quaternion:
        Changed = ImGui::DragFloat4(Name, Floats, Step);
        break;
    case Kind::Color:
        Changed = ImGui::ColorEdit3(Name, Floats, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
        break;
    case Kind::Matrix4:
        memcpy(&Pose, Source, sizeof(Pose));
        ImGui::PushID(Name);
        Changed = EditMatrix(Pose, Selection == Camera && Type.id == hz::SceneTypes::localMatrix);
        ImGui::PopID();
        Value = &Pose;
        break;
    case Kind::String:
        Changed = ImGui::InputText(Name, (char*)Value, Size, ImGuiInputTextFlags_EnterReturnsTrue);
        break;
    case Kind::Enum:
    {
        const char* Preview = "(unknown)";
        for (const hz::EnumValue& Entry : Field.enumValues)
        {
            if (Entry.value == *(int32_t*)Value)
            {
                Preview = Entry.Name;
            }
        }
        if (ImGui::BeginCombo(Name, Preview))
        {
            for (const hz::EnumValue& Entry : Field.enumValues)
            {
                if (ImGui::Selectable(Entry.Name, Entry.value == *(int32_t*)Value))
                {
                    *(int32_t*)Value = Entry.value;
                    Changed = true;
                }
            }
            ImGui::EndCombo();
        }
        break;
    }
    case Kind::AssetReference:
    case Kind::EntityReference:
    {
        hz::Guid Guid;
        memcpy(&Guid, Source, sizeof(Guid));
        char Text[hz::kIDStringCapacity];
        Guid.ToString(Text);
        if (ImGui::InputText(Name, Text, sizeof(Text), ImGuiInputTextFlags_EnterReturnsTrue))
        {
            Changed = Guid.Parse(Text);
            if (Changed)
            {
                memcpy(Value, &Guid, sizeof(Guid));
            }
            else
            {
                FailedField = ID;
            }
        }
        break;
    }
    case Kind::Array:
        return;
    }
    if (Changed)
    {
        const hz::PropertyValue Edit = { .kind = Field.kind, .Data = Value, .Size = Size };
        const bool              Accepted = Array ? World.SetArrayElementProperty(Selection, Type.id, Array->id, Index, Field.id, Edit)
                                                 : World.SetProperty(Selection, Type.id, Field.id, Edit);
        FailedField = Accepted ? 0 : ID;
        if (Accepted && Type.id == hz::SceneTypes::localMatrix && !Array)
        {
            Inspector.EditedPose = Selection;
            Inspector.NewPose = Pose;
        }
        if (Accepted && (Type.id == hz::SceneTypes::name || (Type.id == hz::SceneTypes::light && Field.kind == Kind::Enum)))
        {
            RefreshAt = 0;
        }
    }
    if (FailedField == ID)
    {
        ImGui::TextColored(ImVec4(1, 0.5f, 0.4f, 1), "Invalid value for %s", Name);
    }
}

void SceneInspector::Panel::DrawArray(const hz::TypeDesc& Type, const hz::PropertyDesc& Field, const void* Source)
{
    const uint32_t Count = Field.pArrayOperations->pCount(Source);
    if (!ImGui::TreeNode(Field.Name, "%s [%u]", Field.Name, Count))
    {
        return;
    }
    const ImGuiID ID = ImGui::GetID("Structure");
    int           NewCount = (int)Count;
    if (ImGui::Button("Add element"))
    {
        ++NewCount;
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(110);
    ImGui::InputInt("Length", &NewCount, 0, 0, ImGuiInputTextFlags_EnterReturnsTrue);
    if (NewCount != (int)Count)
    {
        FailedField = NewCount >= 0 && World.ResizeArray(Selection, Type.id, Field.id, (uint32_t)NewCount) ? 0 : ID;
        ++ArrayVersion;
    }
    else
    {
        const hz::TypeDesc& Element = *Field.pElementType;
        ImGui::PushID((int)ArrayVersion);
        for (uint32_t I = 0; I < Count; ++I)
        {
            ImGui::PushID((int)I);
            const bool Expanded = ImGui::TreeNode("element", "Element %u", I);
            ImGui::SameLine();
            const bool Remove = ImGui::SmallButton("Remove");
            if (Expanded)
            {
                if (!Remove)
                {
                    const uint8_t* Data = (const uint8_t*)Field.pArrayOperations->Data(Source) + I * Element.Size;
                    for (const hz::PropertyDesc& Member : Element.properties)
                    {
                        ImGui::BeginDisabled((Member.Flags & hz::PROPERTY_READ_ONLY) != 0);
                        DrawProperty(Type, Member, Data + Member.offset, &Field, I);
                        ImGui::EndDisabled();
                    }
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
            if (Remove)
            {
                FailedField = World.RemoveArrayElement(Selection, Type.id, Field.id, I) ? 0 : ID;
                ++ArrayVersion;
                break;
            }
        }
        ImGui::PopID();
    }
    if (FailedField == ID)
    {
        ImGui::TextDisabled("Invalid array edit");
    }
    ImGui::TreePop();
}
