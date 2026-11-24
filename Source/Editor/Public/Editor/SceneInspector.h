#pragma once

#include "Editor/ViewportPicking.h"
#include "Core/IMath.h"
#include "Core/IUniquePtr.h"
#include "Scene/World.h"

struct WindowHandle;

class SceneInspector
{
public:
    SceneInspector(const WindowHandle& Owner, hz::World& World, const hz::TypeRegistry& Types, hz::Entity Camera,
                   hz::RenderContext& Context, const ViewportGeometry& Geometry);
    ~SceneInspector();

    ViewportPicking  Picking;
    void             OnPresented(hz::SubmitHandle Submission);
    void             Update(const hz::RenderScene& Scene);
    void             RecordOverlay(hz::RenderContext& Context, hz::CommandList& Commands, const hz::GPUTexture& Backbuffer,
                                   const hz::GPUBuffer& Frame, const hz::SceneView& View, const hz::GpuDrawList& Draws,
                                   const hz::GPUSampler& Sampler, bool Transparent, float JitterX, float JitterY);
    uint64_t         TakeViewportClick(uint32_t Width, uint32_t Height, uint32_t& X, uint32_t& Y);
    void             ApplyPick(uint64_t Serial, const hz::RenderScene& Scene, hz::RenderInstanceID Id);
    // Valid until the next Update. Driven transforms consume the edit before scene extraction.
    hz::Entity       EditedPose;
    Matrix4          NewPose;

private:
    struct Panel;
    hz::UniquePtr<Panel> UI;
};
