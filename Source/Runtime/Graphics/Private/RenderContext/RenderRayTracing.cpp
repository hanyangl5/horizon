/* Copyright (c) 2026 Horizon */
#include "Graphics/RenderContext.h"
#include "Core/IContainer.h"
#include "Core/ILog.h"
#include "Core/IToolFileSystem.h"
#include <slang.h>
#include <slang-com-ptr.h>
#include <string.h>
#include "Core/IMemory.h"

namespace hz
{
class AccelerationStructureData
{
public:
    RenderContext*                       pContext = nullptr;
    AccelerationStructure*               pStructure = nullptr;
    AccelerationStructureType            type = ACCELERATION_STRUCTURE_TYPE_BOTTOM;
    AccelerationStructureBuildFlags      flags = ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
    Array<const GPUBuffer*>              inputs;
    Array<AccelerationStructureInstance> instances;
    uint32_t                             capacity = 0;
    uint32_t                             builtCount = 0;
    bool                                 built = false;
};

GPUAccelerationStructure::~GPUAccelerationStructure() { destroy(); }
GPUAccelerationStructure::GPUAccelerationStructure(GPUAccelerationStructure&& other) noexcept: pData(other.pData) { other.pData = nullptr; }
GPUAccelerationStructure& GPUAccelerationStructure::operator=(GPUAccelerationStructure&& other) noexcept
{
    if (this != &other)
    {
        destroy();
        pData = other.pData;
        other.pData = nullptr;
    }
    return *this;
}
void GPUAccelerationStructure::destroy()
{
    if (!pData)
    {
        return;
    }
    removeAccelerationStructure(pData->pContext->pRaytracing, pData->pStructure);
    --pData->pContext->resourceCount;
    tf_delete(pData);
    pData = nullptr;
}
uint32_t GPUAccelerationStructure::getSrvIndex() const { return pData ? getAccelerationStructureSrvIndex(pData->pStructure) : UINT32_MAX; }

bool RenderContext::supportsRayTracing() const
{
    return pRenderer && pRenderer->pGpu->settings.rayPipelineSupported && pRenderer->pGpu->settings.rayQuerySupported;
}

GPUAccelerationStructure RenderContext::createAccelerationStructure(const AccelerationStructureDesc& input)
{
    if (!pRaytracing && !initRaytracing(pRenderer, &pRaytracing))
    {
        LOGF(eERROR, "Ray tracing is not supported by the selected device");
        return {};
    }
    ASSERT(!(input.flags & ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE));
    AccelerationStructureData* data = tf_new(AccelerationStructureData);
    data->pContext = this;
    data->type = input.type;
    data->flags = input.flags;
    ::AccelerationStructureDesc                desc = { .type = input.type, .flags = input.flags };
    Array<::AccelerationStructureGeometryDesc> geometries;
    Array<::AccelerationStructureInstanceDesc> instances;
    if (input.type == ACCELERATION_STRUCTURE_TYPE_BOTTOM)
    {
        ASSERT(input.geometries.count);
        for (const AccelerationStructureGeometry& geometry : input.geometries)
        {
            ASSERT(geometry.pVertexBuffer && geometry.pVertexBuffer->isValid() && geometry.vertexCount);
            geometries.pushBack({ .pVertexBuffer = geometry.pVertexBuffer->pBuffer,
                                  .pIndexBuffer = geometry.pIndexBuffer ? geometry.pIndexBuffer->pBuffer : nullptr,
                                  .vertexOffset = geometry.vertexOffset,
                                  .vertexCount = geometry.vertexCount,
                                  .vertexStride = geometry.vertexStride,
                                  .vertexFormat = geometry.vertexFormat,
                                  .indexOffset = geometry.indexOffset,
                                  .indexCount = geometry.indexCount,
                                  .indexType = geometry.indexType,
                                  .flags = geometry.opaque ? ACCELERATION_STRUCTURE_GEOMETRY_FLAG_OPAQUE
                                                           : ACCELERATION_STRUCTURE_GEOMETRY_FLAG_NO_DUPLICATE_ANYHIT_INVOCATION });
            data->inputs.pushBack(geometry.pVertexBuffer);
            if (geometry.pIndexBuffer)
            {
                data->inputs.pushBack(geometry.pIndexBuffer);
            }
        }
        desc.bottom = { .descCount = geometries.size(), .pGeometryDescs = geometries.data() };
    }
    else
    {
        ASSERT(input.instances.count);
        data->instances = Array<AccelerationStructureInstance>(input.instances);
        data->capacity = input.instances.count;
        for (const AccelerationStructureInstance& instance : input.instances)
        {
            ASSERT(instance.pBottom && instance.pBottom->isValid());
            ::AccelerationStructureInstanceDesc native = { .pBottomAS = instance.pBottom->pData->pStructure,
                                                           .instanceID = instance.instanceId,
                                                           .instanceMask = instance.mask,
                                                           .instanceContributionToHitGroupIndex = instance.hitGroupOffset,
                                                           .flags = instance.flags };
            memcpy(native.transform, instance.transform, sizeof(native.transform));
            instances.pushBack(native);
        }
        desc.top = { .descCount = instances.size(), .pInstanceDescs = instances.data() };
    }
    addAccelerationStructure(pRaytracing, &desc, &data->pStructure);
    if (!data->pStructure)
    {
        tf_delete(data);
        return {};
    }
    ++resourceCount;
    return GPUAccelerationStructure(data);
}

void CommandList::buildAccelerationStructure(GPUAccelerationStructure& structure, Span<AccelerationStructureInstance> instances,
                                             bool update)
{
    ASSERT(structure.isValid());
    AccelerationStructureData& data = *structure.pData;
    ASSERT(data.pContext == pContext);
    ASSERT(!update || (data.built && (data.flags & ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE)));
    RaytracingBuildASDesc desc = { .pAccelerationStructure = data.pStructure, .issueRWBarrier = true, .update = update };
    if (data.type == ACCELERATION_STRUCTURE_TYPE_BOTTOM)
    {
        arrsetlen(bufferBarriers, 0);
        addBufferBarriers({ data.inputs.data(), data.inputs.size() }, false);
        barrier((uint32_t)arrlen(bufferBarriers), bufferBarriers, 0, nullptr, 0, nullptr);
    }
    else
    {
        if (!instances.count)
        {
            instances = { data.instances.data(), data.instances.size() };
        }
        ASSERT(instances.count <= data.capacity && (!update || instances.count == data.builtCount));
        Buffer*                 upload = createUploadBuffer((uint64_t)instances.count * sizeof(RaytracingInstanceData));
        RaytracingInstanceData* output = (RaytracingInstanceData*)upload->pCpuMappedAddress;
        for (uint32_t i = 0; i < instances.count; ++i)
        {
            const AccelerationStructureInstance& instance = instances.pData[i];
            ASSERT(instance.pBottom && instance.pBottom->isValid() && instance.pBottom->pData->built);
            ASSERT(instance.instanceId < (1u << 24) && instance.mask < (1u << 8) && instance.hitGroupOffset < (1u << 24));
            output[i] = { .instanceIdAndMask = instance.instanceId | (instance.mask << 24),
                          .hitGroupAndFlags = instance.hitGroupOffset | ((uint32_t)instance.flags << 24),
                          .address = getAccelerationStructureGpuAddress(instance.pBottom->pData->pStructure) };
            memcpy(output[i].transform, instance.transform, sizeof(output[i].transform));
        }
        desc.pInstanceBuffer = upload;
        desc.instanceCount = instances.count;
        data.builtCount = instances.count;
    }
    cmdBuildAccelerationStructure(pCmd, pContext->pRaytracing, &desc);
    data.built = true;
}

GPURayTracingPipeline::~GPURayTracingPipeline() { destroy(); }
GPURayTracingPipeline::GPURayTracingPipeline(GPURayTracingPipeline&& other) noexcept: pContext(other.pContext), pPipeline(other.pPipeline)
{
    other.pContext = nullptr;
    other.pPipeline = nullptr;
}
GPURayTracingPipeline& GPURayTracingPipeline::operator=(GPURayTracingPipeline&& other) noexcept
{
    if (this != &other)
    {
        destroy();
        pContext = other.pContext;
        pPipeline = other.pPipeline;
        other.pContext = nullptr;
        other.pPipeline = nullptr;
    }
    return *this;
}
void GPURayTracingPipeline::destroy()
{
    if (pPipeline)
    {
        removeRaytracingPipeline(pContext->pRaytracing, pPipeline);
        --pContext->resourceCount;
        pPipeline = nullptr;
        pContext = nullptr;
    }
}

struct RayEntry
{
    const char* name;
    SlangStage  stage;
};
static void addRayEntry(Array<RayEntry>& entries, const char* name, SlangStage stage)
{
    if (!name)
    {
        return;
    }
    for (const RayEntry& entry : entries)
    {
        if (!strcmp(entry.name, name))
        {
            ASSERT(entry.stage == stage);
            return;
        }
    }
    entries.pushBack({ .name = name, .stage = stage });
}
static bool raySlangResult(SlangResult result, const Slang::ComPtr<ISlangBlob>& diagnostics)
{
    if (diagnostics && diagnostics->getBufferSize())
    {
        LOGF(SLANG_FAILED(result) ? eERROR : eWARNING, "Slang ray tracing: %s", (const char*)diagnostics->getBufferPointer());
    }
    return SLANG_SUCCEEDED(result);
}

GPURayTracingPipeline RenderContext::createRayTracingPipeline(const RayTracingPipelineDesc& input)
{
    ASSERT(input.pFileName && input.pRayGeneration && input.rootConstantSize && input.rootConstantSize <= 256 &&
           !(input.rootConstantSize % 4));
    if (!pRaytracing && !initRaytracing(pRenderer, &pRaytracing))
    {
        return {};
    }
    if (!pSlangSession && SLANG_FAILED(slang::createGlobalSession(&pSlangSession)))
    {
        return {};
    }
    FileStream stream = {};
    if (!fsOpenStreamFromPath(input.sourceDirectory, input.pFileName, FM_READ, &stream))
    {
        LOGF(eERROR, "Cannot open ray tracing shader '%s'", input.pFileName);
        return {};
    }
    const ssize_t size = fsGetStreamFileSize(&stream);
    Array<char>   source(size > 0 && size < UINT32_MAX ? (uint32_t)size + 1 : 0);
    const bool    read = size > 0 && size < UINT32_MAX && fsReadFromStream(&stream, source.data(), (size_t)size) == (size_t)size;
    fsCloseStream(&stream);
    if (!read)
    {
        return {};
    }
    source[(uint32_t)size] = 0;
    char        path[FS_MAX_PATH];
    const char* searchPath = fsGetResourceDirectory(input.sourceDirectory);
    if (!fsAppendPathComponent(searchPath, input.pFileName, path))
    {
        return {};
    }
    const slang::CompilerOptionEntry options[] = {
        { .name = slang::CompilerOptionName::NoMangle, .value = { .intValue0 = 1 } },
    };
    const slang::TargetDesc        target = { .format = SLANG_DXIL,
                                              .profile = pSlangSession->findProfile("sm_6_6"),
                                              .flags = SLANG_TARGET_FLAG_GENERATE_WHOLE_PROGRAM };
    const slang::SessionDesc       sessionDesc = { .targets = &target,
                                                   .targetCount = 1,
                                                   .defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR,
                                                   .searchPaths = &searchPath,
                                                   .searchPathCount = 1,
                                                   .compilerOptionEntries = options,
                                                   .compilerOptionEntryCount = 1 };
    Slang::ComPtr<slang::ISession> session;
    Slang::ComPtr<ISlangBlob>      diagnostics;
    if (SLANG_FAILED(pSlangSession->createSession(sessionDesc, session.writeRef())))
    {
        return {};
    }
    slang::IModule* module = session->loadModuleFromSourceString("HorizonRayTracing", path, source.data(), diagnostics.writeRef());
    if (!raySlangResult(module ? SLANG_OK : SLANG_FAIL, diagnostics))
    {
        return {};
    }
    Array<RayEntry> entries;
    addRayEntry(entries, input.pRayGeneration, SLANG_STAGE_RAY_GENERATION);
    for (const char* name : input.missShaders)
    {
        addRayEntry(entries, name, SLANG_STAGE_MISS);
    }
    Array<RaytracingHitGroupDesc> groups;
    for (const RayTracingHitGroup& group : input.hitGroups)
    {
        addRayEntry(entries, group.pClosestHit, SLANG_STAGE_CLOSEST_HIT);
        addRayEntry(entries, group.pAnyHit, SLANG_STAGE_ANY_HIT);
        addRayEntry(entries, group.pIntersection, SLANG_STAGE_INTERSECTION);
        groups.pushBack(
            { .pName = group.pName, .pClosestHit = group.pClosestHit, .pAnyHit = group.pAnyHit, .pIntersection = group.pIntersection });
    }
    Array<Slang::ComPtr<slang::IEntryPoint>> entryPoints(entries.size());
    Array<slang::IComponentType*>            components;
    components.pushBack(module);
    for (uint32_t i = 0; i < entries.size(); ++i)
    {
        if (!raySlangResult(
                module->findAndCheckEntryPoint(entries[i].name, entries[i].stage, entryPoints[i].writeRef(), diagnostics.writeRef()),
                diagnostics))
        {
            return {};
        }
        components.pushBack(entryPoints[i].get());
    }
    Slang::ComPtr<slang::IComponentType> program, linked;
    Slang::ComPtr<ISlangBlob>            library;
    if (!raySlangResult(
            session->createCompositeComponentType(components.data(), components.size(), program.writeRef(), diagnostics.writeRef()),
            diagnostics) ||
        !raySlangResult(program->link(linked.writeRef(), diagnostics.writeRef()), diagnostics) ||
        !raySlangResult(linked->getTargetCode(0, library.writeRef(), diagnostics.writeRef()), diagnostics))
    {
        return {};
    }
    const RaytracingPipelineDesc desc = { .pLibrary = library->getBufferPointer(),
                                          .librarySize = library->getBufferSize(),
                                          .pRayGeneration = input.pRayGeneration,
                                          .ppMissShaders = input.missShaders.pData,
                                          .missShaderCount = input.missShaders.count,
                                          .pHitGroups = groups.data(),
                                          .hitGroupCount = groups.size(),
                                          .payloadSize = input.payloadSize,
                                          .attributeSize = input.attributeSize,
                                          .maxRecursionDepth = input.maxRecursionDepth,
                                          .rootConstantSize = input.rootConstantSize };
    RaytracingPipeline*          pipeline = nullptr;
    addRaytracingPipeline(pRaytracing, &desc, &pipeline);
    if (!pipeline)
    {
        return {};
    }
    ++resourceCount;
    return GPURayTracingPipeline(this, pipeline);
}

void CommandList::setPipeline(const GPURayTracingPipeline& pipeline)
{
    ASSERT(pipeline.isValid() && pipeline.pContext == pContext);
    pCurrentRootSignature = getRaytracingPipelineRootSignature(pipeline.pPipeline);
    pCurrentDrawIndirectSignature = nullptr;
    cmdBindRaytracingPipeline(pCmd, pipeline.pPipeline);
}
void CommandList::traceRays(const GPURayTracingPipeline& pipeline, uint32_t width, uint32_t height, uint32_t depth,
                            const Dependencies& dependencies)
{
    ASSERT(pipeline.isValid() && width && height && depth);
    barrier(dependencies, nullptr);
    cmdTraceRays(pCmd, pipeline.pPipeline, width, height, depth);
}
} // namespace hz
