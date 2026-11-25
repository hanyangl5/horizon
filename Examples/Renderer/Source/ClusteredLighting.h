#pragma once

#include "../Shaders/ClusterGrid.h"
#include "Scene/RenderScene.h"
#include "Graphics/FrameGraph.h"
#include "Graphics/FrameUpload.h"

struct FrameData;
struct LightingFrameData;

class ClusteredLighting
{
public:
    static constexpr uint32_t MaxClusteredLights = CLUSTER_MAX_LIGHTS;

    explicit ClusteredLighting(hz::RenderContext& Context);
    static hz::FrameGraphBufferDesc GetTileMaskDesc();
    bool                            Prepare(LightingFrameData& Frame, hz::FrameUpload& Upload, const hz::GPUBuffer& TileLightBitMasks,
                                            hz::Span<hz::RenderLight> Lights, hz::Span<uint32_t> Active, const FrameData& Camera);
    void BuildLightTileMasks(hz::CommandList& Commands, const FrameData& Camera, const hz::GPUBuffer& TileLightBitMasks,
                             const hz::GPUBuffer& Lights, const hz::GPUBuffer& ActiveLights, uint32_t ActiveLocalLightCount) const;

    hz::Span<uint32_t> GetLightIndices() const { return { Indices.GetData(), Indices.Size() }; }

private:
    struct LocalLightData
    {
        float    Depth;
        uint32_t Index;
    };
    struct Bin
    {
        uint32_t First, Last;
    };
    static int Compare(const void* A, const void* B);

    hz::GPUPipeline           Pipeline;
    hz::Array<LocalLightData> SortedLightList;
    hz::Array<uint32_t>       Indices;
    Bin                       Bins[CLUSTER_DEPTH_BIN_COUNT];
};
