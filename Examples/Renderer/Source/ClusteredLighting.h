#pragma once

#include "Scene/RenderScene.h"
#include "Graphics/RenderContext.h"

class ClusteredLighting
{
public:
    explicit ClusteredLighting(hz::RenderContext& Context);
    bool Prepare(uint32_t Slot, hz::Span<hz::RenderLight> Lights, hz::Span<uint32_t> Active, uint32_t MainDirectional,
                 const Matrix4& ViewProjection, float Near, float Far, uint32_t Width, uint32_t Height);
    void BuildMasks(hz::CommandList& Commands, uint32_t Slot) const;

    const hz::GPUBuffer& GetData(uint32_t Slot) const { return Frames[Slot].Data; }
    const hz::GPUBuffer* GetMask(uint32_t Slot) const { return Frames[Slot].Words ? &Frames[Slot].Mask : nullptr; }

private:
    struct Frame
    {
        hz::GPUBuffer Data, Mask;
        uint32_t      TilesX = 0, TilesY = 0, Words = 0;
    };
    struct Candidate
    {
        float    Depth;
        uint32_t Index;
    };
    struct Bin
    {
        uint32_t First, Last;
    };
    static int Compare(const void* A, const void* B);

    hz::RenderContext&       Context;
    hz::GPUPipeline          Pipeline;
    hz::FixedArray<Frame, 3> Frames;
    hz::Array<Candidate>     Candidates;
    hz::Array<uint32_t>      Indices;
    hz::Array<Vector4>       Spheres;
    Bin                      Bins[1024];
};
