/* Copyright (c) 2026 Horizon */
#include "Graphics/RenderContext.h"
#include "SlangSession.h"
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
    Array<RaytracingInstanceData>        instances;
    Array<AccelerationStructureData*>    bottoms;
    uint32_t                             references = 1;
    void                                 setInstance(uint32_t index, const AccelerationStructureInstance& instance)
    {
        ASSERT(index < instances.Size() && instance.pBottom && instance.pBottom->isValid());
        ASSERT(instance.instanceId < (1u << 24) && instance.mask < (1u << 8) && instance.hitGroupOffset < (1u << 24));
        AccelerationStructureData* bottom = instance.pBottom->Data;
        ASSERT(bottom->pContext == pContext && bottom->type == ACCELERATION_STRUCTURE_TYPE_BOTTOM);
        RaytracingInstanceData next = { .instanceIdAndMask = instance.instanceId | (instance.mask << 24),
                                        .hitGroupAndFlags = instance.hitGroupOffset | ((uint32_t)instance.flags << 24),
                                        .address = getAccelerationStructureGpuAddress(bottom->pStructure) };
        memcpy(next.transform, instance.transform, sizeof(next.transform));
        dirty |= memcmp(&instances[index], &next, sizeof(next)) != 0;
        instances[index] = next;
        if (bottoms[index] != bottom)
        {
            ++bottom->references;
            if (bottoms[index])
            {
                pContext->releaseAccelerationStructure(bottoms[index]);
            }
            bottoms[index] = bottom;
            dirty = true;
        }
    }
    void setInstances(Span<AccelerationStructureInstance> source)
    {
        dirty |= instances.Size() != source.Count;
        for (uint32_t i = source.Count; i < bottoms.Size(); ++i)
        {
            if (bottoms[i])
            {
                pContext->releaseAccelerationStructure(bottoms[i]);
            }
        }
        instances.resize(source.Count);
        bottoms.resize(source.Count);
        for (uint32_t i = 0; i < source.Count; ++i)
        {
            setInstance(i, source.Data[i]);
        }
    }
    uint32_t                             Capacity = 0;
    uint32_t                             builtCount = 0;
    bool                                 built = false;
    bool                                 dirty = true;
};

GPUAccelerationStructure::~GPUAccelerationStructure() { destroy(); }
GPUAccelerationStructure::GPUAccelerationStructure(GPUAccelerationStructure&& other) noexcept: Data(other.Data) { other.Data = nullptr; }
GPUAccelerationStructure& GPUAccelerationStructure::operator=(GPUAccelerationStructure&& other) noexcept
{
    if (this != &other)
    {
        destroy();
        Data = other.Data;
        other.Data = nullptr;
    }
    return *this;
}
void GPUAccelerationStructure::destroy()
{
    if (!Data)
    {
        return;
    }
    Data->pContext->releaseAccelerationStructure(Data);
    Data = nullptr;
}
uint32_t GPUAccelerationStructure::getSrvIndex() const { return Data ? getAccelerationStructureSrvIndex(Data->pStructure) : UINT32_MAX; }

uint32_t GPUAccelerationStructure::getInstanceCount() const { return Data ? Data->instances.Size() : 0; }

void GPUAccelerationStructure::setInstance(uint32_t index, const AccelerationStructureInstance& instance)
{
    ASSERT(Data && Data->type == ACCELERATION_STRUCTURE_TYPE_TOP);
    Data->setInstance(index, instance);
}

void RenderContext::releaseAccelerationStructure(AccelerationStructureData* data)
{
    ASSERT(data->references);
    if (--data->references == 0)
    {
        deferResource(ResourceType::AccelerationStructure, data);
    }
}

void RenderContext::destroyAccelerationStructure(AccelerationStructureData* data)
{
    removeAccelerationStructure(pRaytracing, data->pStructure);
    for (AccelerationStructureData* bottom : data->bottoms)
    {
        if (bottom)
        {
            releaseAccelerationStructure(bottom);
        }
    }
    MemDelete(data);
}

bool RenderContext::supportsRayTracing() const
{
    return pRenderer && pRenderer->pGpu->settings.rayPipelineSupported && pRenderer->pGpu->settings.rayQuerySupported;
}

GPUAccelerationStructure RenderContext::createAccelerationStructure(const AccelerationStructureDesc& input)
{
    if (!pRaytracing && !initRaytracing(pRenderer, &pRaytracing))
    {
        LOGF(Error, "Ray tracing is not supported by the selected device");
        return {};
    }
    ASSERT(!(input.flags & ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE));
    AccelerationStructureData* data = MemNew(AccelerationStructureData);
    data->pContext = this;
    data->type = input.type;
    data->flags = input.flags;
    ::AccelerationStructureDesc                desc = { .type = input.type, .flags = input.flags };
    Array<::AccelerationStructureGeometryDesc> geometries;
    Array<::AccelerationStructureInstanceDesc> instances;
    if (input.type == ACCELERATION_STRUCTURE_TYPE_BOTTOM)
    {
        ASSERT(input.geometries.Count);
        for (const AccelerationStructureGeometry& geometry : input.geometries)
        {
            ASSERT(geometry.pVertexBuffer && geometry.pVertexBuffer->isValid() && geometry.vertexCount);
            geometries.PushBack({ .pVertexBuffer = geometry.pVertexBuffer->pBuffer,
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
            data->inputs.PushBack(geometry.pVertexBuffer);
            if (geometry.pIndexBuffer)
            {
                data->inputs.PushBack(geometry.pIndexBuffer);
            }
        }
        desc.bottom = { .descCount = geometries.Size(), .pGeometryDescs = geometries.GetData() };
    }
    else
    {
        data->Capacity = TF_MAX(input.instances.Count, input.instanceCount);
        ASSERT(data->Capacity);
        data->setInstances(input.instances);
        if (!input.instances.Count)
        {
            data->instances.resize(data->Capacity);
            data->bottoms.resize(data->Capacity);
        }
        for (const AccelerationStructureInstance& instance : input.instances)
        {
            ASSERT(instance.pBottom && instance.pBottom->isValid());
            ::AccelerationStructureInstanceDesc native = { .pBottomAS = instance.pBottom->Data->pStructure,
                                                           .instanceID = instance.instanceId,
                                                           .instanceMask = instance.mask,
                                                           .instanceContributionToHitGroupIndex = instance.hitGroupOffset,
                                                           .flags = instance.flags };
            memcpy(native.transform, instance.transform, sizeof(native.transform));
            instances.PushBack(native);
        }
        ASSERT(!input.instances.Count || input.instanceCount <= input.instances.Count);
        desc.top = { .descCount = data->Capacity, .pInstanceDescs = instances.GetData() };
    }
    addAccelerationStructure(pRaytracing, &desc, &data->pStructure);
    if (!data->pStructure)
    {
        for (AccelerationStructureData* bottom : data->bottoms)
        {
            if (bottom)
            {
                releaseAccelerationStructure(bottom);
            }
        }
        MemDelete(data);
        return {};
    }
    ++resourceCount;
    return GPUAccelerationStructure(data);
}

void CommandList::buildAccelerationStructure(GPUAccelerationStructure& structure, Span<AccelerationStructureInstance> instances,
                                             bool update)
{
    ASSERT(structure.isValid());
    AccelerationStructureData& data = *structure.Data;
    ASSERT(data.pContext == pContext);
    ASSERT(!update || (data.built && (data.flags & ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE)));
    RaytracingBuildASDesc desc = { .pAccelerationStructure = data.pStructure, .issueRWBarrier = true, .update = update };
    if (data.type == ACCELERATION_STRUCTURE_TYPE_BOTTOM)
    {
        arrsetlen(bufferBarriers, 0);
        addBufferBarriers({ data.inputs.GetData(), data.inputs.Size() }, false);
        barrier((uint32_t)arrlen(bufferBarriers), bufferBarriers, 0, nullptr, 0, nullptr);
    }
    else
    {
        if (instances.Count)
        {
            data.setInstances(instances);
        }
        ASSERT(data.instances.Size() <= data.Capacity && (!update || data.instances.Size() == data.builtCount));
        for (AccelerationStructureData* bottom : data.bottoms)
        {
            ASSERT(bottom && bottom->built);
        }
        Buffer* upload = createUploadBuffer((uint64_t)data.instances.Size() * sizeof(RaytracingInstanceData));
        memcpy(upload->pCpuMappedAddress, data.instances.GetData(), data.instances.Size() * sizeof(RaytracingInstanceData));
        desc.pInstanceBuffer = upload;
        desc.instanceCount = data.instances.Size();
        data.builtCount = data.instances.Size();
    }
    cmdBuildAccelerationStructure(pCmd, pContext->pRaytracing, &desc);
    data.built = true;
    data.dirty = false;
}

void CommandList::updateAccelerationStructure(GPUAccelerationStructure& structure)
{
    ASSERT(structure.isValid() && structure.Data->pContext == pContext);
    AccelerationStructureData& data = *structure.Data;
    ASSERT(data.type == ACCELERATION_STRUCTURE_TYPE_TOP);
    if (data.dirty)
    {
        const bool update =
            data.built && (data.flags & ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE) && data.instances.Size() == data.builtCount;
        buildAccelerationStructure(structure, {}, update);
    }
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
        pContext->deferResource(RenderContext::ResourceType::RayTracingPipeline, pPipeline);
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
    entries.PushBack({ .name = name, .stage = stage });
}
static bool raySlangResult(SlangResult result, const Slang::ComPtr<ISlangBlob>& diagnostics)
{
    if (diagnostics && diagnostics->getBufferSize())
    {
        LOGF(SLANG_FAILED(result) ? Error : Warning, "Slang ray tracing: %s", (const char*)diagnostics->getBufferPointer());
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
    slang::IGlobalSession* pSlangSession = getSlangGlobalSession();
    if (!pSlangSession)
    {
        return {};
    }
    FileStream stream = {};
    if (!OpenStreamFromPath(input.sourceDirectory, input.pFileName, FM_READ, &stream))
    {
        LOGF(Error, "Cannot open ray tracing shader '%s'", input.pFileName);
        return {};
    }
    const ssize_t Size = GetStreamFileSize(&stream);
    Array<char>   source(Size > 0 && Size < UINT32_MAX ? (uint32_t)Size + 1 : 0);
    const bool    read = Size > 0 && Size < UINT32_MAX && ReadFromStream(&stream, source.GetData(), (size_t)Size) == (size_t)Size;
    CloseStream(&stream);
    if (!read)
    {
        return {};
    }
    source[(uint32_t)Size] = 0;
    char        path[FS_MAX_PATH];
    const char* searchPath = GetResourceDirectory(input.sourceDirectory);
    if (!AppendPathComponent(searchPath, input.pFileName, path))
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
    slang::IModule* module = session->loadModuleFromSourceString("HorizonRayTracing", path, source.GetData(), diagnostics.writeRef());
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
        groups.PushBack(
            { .pName = group.pName, .pClosestHit = group.pClosestHit, .pAnyHit = group.pAnyHit, .pIntersection = group.pIntersection });
    }
    Array<Slang::ComPtr<slang::IEntryPoint>> entryPoints(entries.Size());
    Array<slang::IComponentType*>            components;
    components.PushBack(module);
    for (uint32_t i = 0; i < entries.Size(); ++i)
    {
        if (!raySlangResult(
                module->findAndCheckEntryPoint(entries[i].name, entries[i].stage, entryPoints[i].writeRef(), diagnostics.writeRef()),
                diagnostics))
        {
            return {};
        }
        components.PushBack(entryPoints[i].get());
    }
    Slang::ComPtr<slang::IComponentType> program, linked;
    Slang::ComPtr<ISlangBlob>            library;
    if (!raySlangResult(
            session->createCompositeComponentType(components.GetData(), components.Size(), program.writeRef(), diagnostics.writeRef()),
            diagnostics) ||
        !raySlangResult(program->link(linked.writeRef(), diagnostics.writeRef()), diagnostics) ||
        !raySlangResult(linked->getTargetCode(0, library.writeRef(), diagnostics.writeRef()), diagnostics))
    {
        return {};
    }
    const RaytracingPipelineDesc desc = { .pLibrary = library->getBufferPointer(),
                                          .librarySize = library->getBufferSize(),
                                          .pRayGeneration = input.pRayGeneration,
                                          .ppMissShaders = input.missShaders.Data,
                                          .missShaderCount = input.missShaders.Count,
                                          .pHitGroups = groups.GetData(),
                                          .hitGroupCount = groups.Size(),
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
