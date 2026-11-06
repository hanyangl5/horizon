/* Copyright (c) 2026 Horizon */
#pragma once

#include "Core/IFileSystem.h"
#include "Core/ISpan.h"
#include "RHI/IRay.h"

namespace hz
{
class RenderContext;
class CommandList;
class GPUBuffer;
class GPUAccelerationStructure;
class AccelerationStructureData;

struct AccelerationStructureGeometry
{
    const GPUBuffer* pVertexBuffer = nullptr;
    const GPUBuffer* pIndexBuffer = nullptr;
    uint64_t         vertexOffset = 0;
    uint64_t         indexOffset = 0;
    uint32_t         vertexCount = 0;
    uint32_t         vertexStride = 12;
    uint32_t         indexCount = 0;
    Format           vertexFormat = Format::R32G32B32_SFLOAT;
    IndexType        indexType = INDEX_TYPE_UINT32;
    bool             opaque = true;
};

struct AccelerationStructureInstance
{
    const GPUAccelerationStructure*    pBottom = nullptr;
    float                              transform[12] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0 };
    uint32_t                           instanceId = 0;
    uint32_t                           mask = 0xff;
    uint32_t                           hitGroupOffset = 0;
    AccelerationStructureInstanceFlags flags = ACCELERATION_STRUCTURE_INSTANCE_FLAG_NONE;
};

struct AccelerationStructureDesc
{
    AccelerationStructureType           type = ACCELERATION_STRUCTURE_TYPE_BOTTOM;
    AccelerationStructureBuildFlags     flags = ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
    Span<AccelerationStructureGeometry> geometries;
    Span<AccelerationStructureInstance> instances;
};

class GPUAccelerationStructure
{
public:
    GPUAccelerationStructure() = default;
    ~GPUAccelerationStructure();
    GPUAccelerationStructure(GPUAccelerationStructure&&) noexcept;
    GPUAccelerationStructure& operator=(GPUAccelerationStructure&&) noexcept;
    GPUAccelerationStructure(const GPUAccelerationStructure&) = delete;
    GPUAccelerationStructure& operator=(const GPUAccelerationStructure&) = delete;
    bool                      isValid() const { return pData != nullptr; }
    uint32_t                  getSrvIndex() const;

private:
    explicit GPUAccelerationStructure(AccelerationStructureData* data): pData(data) {}
    void                       destroy();
    AccelerationStructureData* pData = nullptr;
    friend class RenderContext;
    friend class CommandList;
};

struct RayTracingHitGroup
{
    const char* pName = nullptr;
    const char* pClosestHit = nullptr;
    const char* pAnyHit = nullptr;
    const char* pIntersection = nullptr;
};

struct RayTracingPipelineDesc
{
    ResourceDirectory        sourceDirectory = RD_SHADER_SOURCES;
    const char*              pFileName = nullptr;
    const char*              pRayGeneration = nullptr;
    Span<const char*>        missShaders;
    Span<RayTracingHitGroup> hitGroups;
    uint32_t                 payloadSize = 32;
    uint32_t                 attributeSize = 8;
    uint32_t                 maxRecursionDepth = 2;
    uint32_t                 rootConstantSize = 128;
    const char*              pName = nullptr;
};

class GPURayTracingPipeline
{
public:
    GPURayTracingPipeline() = default;
    ~GPURayTracingPipeline();
    GPURayTracingPipeline(GPURayTracingPipeline&&) noexcept;
    GPURayTracingPipeline& operator=(GPURayTracingPipeline&&) noexcept;
    GPURayTracingPipeline(const GPURayTracingPipeline&) = delete;
    GPURayTracingPipeline& operator=(const GPURayTracingPipeline&) = delete;
    bool                   isValid() const { return pPipeline != nullptr; }

private:
    GPURayTracingPipeline(RenderContext* context, RaytracingPipeline* pipeline): pContext(context), pPipeline(pipeline) {}
    void                destroy();
    RenderContext*      pContext = nullptr;
    RaytracingPipeline* pPipeline = nullptr;
    friend class RenderContext;
    friend class CommandList;
};
} // namespace hz
