#pragma once

#include "Core/IContainer.h"
#include "Core/IUniquePtr.h"
#include "SceneView.h"

namespace hz
{
class MeshAsset;
}

class RayTracingScene
{
public:
    explicit RayTracingScene(hz::RenderContext& context): context(context) {}

    // The caller must wait for this frame's previous submission before updating it.
    void                                update(hz::CommandList& commands, const SceneView& view);
    const hz::GPUAccelerationStructure& getSceneTLAS(uint32_t frameIndex) const { return frames[frameIndex].tlas; }
    void                                addBuffer(const hz::GPUBuffer* buffer);

    hz::Array<const hz::GPUBuffer*>  buffers;
    hz::Array<const hz::GPUTexture*> textures;

private:
    static constexpr uint32_t frameCount = 3;

    struct BLAS
    {
        hz::GpuSubmeshRef                mesh;
        hz::GPUAccelerationStructure     structure;
        bool                             opaque = true;
        hz::FixedArray<bool, frameCount> usedByFrame = {};
    };

    struct Frame
    {
        hz::GPUAccelerationStructure                 tlas;
        hz::Array<hz::AccelerationStructureInstance> instances;

        bool haveInstancesChanged(hz::Span<hz::AccelerationStructureInstance> current) const;
    };

    BLAS& getBLAS(hz::CommandList& commands, hz::GpuSubmeshRef meshRef, const hz::MeshAsset& mesh, bool opaque);
    void  collectInstances(hz::CommandList& commands, const SceneView& view);
    void  updateTLAS(hz::CommandList& commands, Frame& frame);
    void  collectTextures(const SceneView& view);
    void  removeUnusedBLAS();

    hz::RenderContext&                           context;
    // Stable addresses are required by the TLAS instance descriptions.
    hz::Array<hz::unique_ptr<BLAS>>              bottomLevels;
    hz::FixedArray<Frame, frameCount>            frames;
    hz::Array<hz::AccelerationStructureInstance> instances;
};
