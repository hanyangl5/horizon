#pragma once

#include "Scene/GpuDrawList.h"

class SceneInspector;

struct ViewportGeometry
{
    const char* Shader;
    hz::Format  DepthFormat;
    uint32_t    BindingSlot;
    uint32_t    DrawSlot;
};

class ViewportPicking
{
public:
    ViewportPicking(hz::RenderContext& Context, const ViewportGeometry& Geometry);
    void Cancel();
    void Update(SceneInspector& Inspector, const hz::RenderScene& Scene);
    bool HasWork(hz::RenderPrimitiveID Selected) const;
    void OnSubmitted(hz::SubmitHandle Submission);
    void Record(hz::CommandList& Cmd, hz::BufferSrv Frame, const hz::SceneView& View, const hz::GpuDrawList& Draws,
                const hz::GPUSampler& Sampler, bool TransparentEnabled, hz::RenderPrimitiveID Selected, const hz::GPUTexture& Backbuffer,
                float JitterX, float JitterY);

private:
    void PreparePicking();
    void PrepareOutline(const hz::GPUTexture& Backbuffer);
    void DrawIds(hz::CommandList& Cmd, hz::BufferSrv Frame, const hz::SceneView& View, const hz::GpuDrawList& Draws,
                 const hz::GPUSampler& Sampler, bool TransparentEnabled, hz::RenderPrimitiveID Selected, const hz::GPUTexture& Target,
                 const hz::GPUTexture& Depth, uint32_t OriginX, uint32_t OriginY);
    struct PickReadback
    {
        hz::GPUBuffer    Buffer;
        hz::SubmitHandle Submission;
        uint64_t         Serial = 0;
    };

    hz::RenderContext&              Context;
    ViewportGeometry                Geometry;
    hz::GPUPipeline                 PickPipeline, PickReadPipeline, MaskPipeline, OutlinePipeline;
    hz::GPUTexture                  PickIds, PickDepth, SelectionMask, SelectionDepth;
    hz::GPUSampler                  OutlineSampler;
    hz::Format                      OutlineFormat = hz::Format::UNDEFINED;
    hz::GPUBuffer                   PickOutput;
    hz::FixedArray<PickReadback, 3> PickResults;
    PickReadback*                   RecordedPick = nullptr;
    uint64_t                        PendingPick = 0;
    uint32_t                        PickX = 0, PickY = 0;
};
