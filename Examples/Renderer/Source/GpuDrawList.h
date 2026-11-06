#pragma once
#include "Core/IContainer.h"
#include "Graphics/RenderContext.h"
#include "Scene/SceneFrame.h"

struct GpuDrawRecord
{
    uint32_t instance;
    uint32_t material;
    uint32_t materialId;
    uint32_t alphaMode;
    uint32_t positions;
    uint32_t normals;
    uint32_t texcoords;
    uint32_t indices;
    uint32_t indexStride;
    uint32_t firstIndex;
    int32_t  vertexOffset;
    uint32_t indexCount;
};

class GpuDrawList
{
public:
    explicit GpuDrawList(hz::RenderContext& context);
    void                 prepare(hz::CommandList&, const hz::GPUBuffer& frame, const hz::SceneFrameSnapshot&, bool culling = true);
    const hz::GPUBuffer& getRecords(uint32_t backing) const { return frames[backing].records; }
    const hz::GPUBuffer& getCommands(uint32_t backing, bool transparent = false) const
    {
        return transparent ? frames[backing].transparent : frames[backing].opaque;
    }
    const hz::GPUBuffer& getShadowCommands(uint32_t backing) const { return frames[backing].shadow; }
    hz::Dependencies getDependencies(const hz::GPUBuffer& frame, const hz::SceneFrameSnapshot& scene, const hz::GPUBuffer& indirect) const;
    const hz::GPUBuffer& getStatistics(uint32_t backing) const { return frames[backing].statistics; }
    uint32_t             getCount(uint32_t backing) const { return frames[backing].count; }

private:
    struct Frame
    {
        hz::GPUBuffer records;
        hz::GPUBuffer opaque;
        hz::GPUBuffer transparent;
        hz::GPUBuffer shadow;
        hz::GPUBuffer statistics;
        uint32_t      capacity = 0;
        uint32_t      count = 0;
    };
    hz::RenderContext&                       context;
    hz::GPUPipeline                          pipeline;
    hz::GPUPipeline                          clearPipeline;
    hz::FixedArray<Frame, 3>                 frames;
    hz::Array<GpuDrawRecord>                 records;
    hz::Array<uint32_t>                      materialIds;
    uint64_t                                 materialRevision = 0;
    mutable hz::Array<const hz::GPUBuffer*>  buffers;
    mutable hz::Array<const hz::GPUTexture*> textures;
    mutable const hz::GPUBuffer*             indirectDependency = nullptr;
};
