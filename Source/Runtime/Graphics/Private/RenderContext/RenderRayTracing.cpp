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
    AccelerationStructureType            Type = ACCELERATION_STRUCTURE_TYPE_BOTTOM;
    AccelerationStructureBuildFlags      Flags = ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
    Array<const GPUBuffer*>              inputs;
    Array<RaytracingInstanceData>        instances;
    Array<AccelerationStructureData*>    bottoms;
    uint32_t                             references = 1;
    void                                 SetInstance(uint32_t IndexValue, const AccelerationStructureInstance& Instance)
    {
        ASSERT(IndexValue < instances.Size() && Instance.pBottom && Instance.pBottom->IsValid());
        ASSERT(Instance.instanceId < (1u << 24) && Instance.mask < (1u << 8) && Instance.hitGroupOffset < (1u << 24));
        AccelerationStructureData* Bottom = Instance.pBottom->Data;
        ASSERT(Bottom->pContext == pContext && Bottom->Type == ACCELERATION_STRUCTURE_TYPE_BOTTOM);
        RaytracingInstanceData Next = { .instanceIdAndMask = Instance.instanceId | (Instance.mask << 24),
                                        .hitGroupAndFlags = Instance.hitGroupOffset | ((uint32_t)Instance.Flags << 24),
                                        .address = getAccelerationStructureGpuAddress(Bottom->pStructure) };
        memcpy(Next.transform, Instance.transform, sizeof(Next.transform));
        dirty |= memcmp(&instances[IndexValue], &Next, sizeof(Next)) != 0;
        instances[IndexValue] = Next;
        if (bottoms[IndexValue] != Bottom)
        {
            ++Bottom->references;
            if (bottoms[IndexValue])
            {
                pContext->ReleaseAccelerationStructure(bottoms[IndexValue]);
            }
            bottoms[IndexValue] = Bottom;
            dirty = true;
        }
    }
    void SetInstances(Span<AccelerationStructureInstance> Source)
    {
        dirty |= instances.Size() != Source.Count;
        for (uint32_t I = Source.Count; I < bottoms.Size(); ++I)
        {
            if (bottoms[I])
            {
                pContext->ReleaseAccelerationStructure(bottoms[I]);
            }
        }
        instances.Resize(Source.Count);
        bottoms.Resize(Source.Count);
        for (uint32_t I = 0; I < Source.Count; ++I)
        {
            SetInstance(I, Source.Data[I]);
        }
    }
    uint32_t                             Capacity = 0;
    uint32_t                             builtCount = 0;
    bool                                 built = false;
    bool                                 dirty = true;
};

GPUAccelerationStructure::~GPUAccelerationStructure() { Destroy(); }
GPUAccelerationStructure::GPUAccelerationStructure(GPUAccelerationStructure&& Other) noexcept: Data(Other.Data) { Other.Data = nullptr; }
GPUAccelerationStructure& GPUAccelerationStructure::operator=(GPUAccelerationStructure&& Other) noexcept
{
    if (this != &Other)
    {
        Destroy();
        Data = Other.Data;
        Other.Data = nullptr;
    }
    return *this;
}
void GPUAccelerationStructure::Destroy()
{
    if (!Data)
    {
        return;
    }
    Data->pContext->ReleaseAccelerationStructure(Data);
    Data = nullptr;
}
uint32_t GPUAccelerationStructure::GetSrvIndex() const { return Data ? getAccelerationStructureSrvIndex(Data->pStructure) : UINT32_MAX; }

uint32_t GPUAccelerationStructure::GetInstanceCount() const { return Data ? Data->instances.Size() : 0; }

void GPUAccelerationStructure::SetInstance(uint32_t IndexValue, const AccelerationStructureInstance& Instance)
{
    ASSERT(Data && Data->Type == ACCELERATION_STRUCTURE_TYPE_TOP);
    Data->SetInstance(IndexValue, Instance);
}

void RenderContext::ReleaseAccelerationStructure(AccelerationStructureData* DataValue)
{
    ASSERT(DataValue->references);
    if (--DataValue->references == 0)
    {
        DeferResource(ResourceType::AccelerationStructure, DataValue);
    }
}

void RenderContext::DestroyAccelerationStructure(AccelerationStructureData* DataValue)
{
    removeAccelerationStructure(pRaytracing, DataValue->pStructure);
    for (AccelerationStructureData* Bottom : DataValue->bottoms)
    {
        if (Bottom)
        {
            ReleaseAccelerationStructure(Bottom);
        }
    }
    MemDelete(DataValue);
}

bool RenderContext::SupportsRayTracing() const
{
    return pRenderer && pRenderer->pGpu->settings.rayPipelineSupported && pRenderer->pGpu->settings.rayQuerySupported;
}

GPUAccelerationStructure RenderContext::CreateAccelerationStructure(const AccelerationStructureDesc& Input)
{
    if (!pRaytracing && !initRaytracing(pRenderer, &pRaytracing))
    {
        LOGF(Error, "Ray tracing is not supported by the selected device");
        return {};
    }
    ASSERT(!(Input.Flags & ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE));
    AccelerationStructureData* DataValue = MemNew(AccelerationStructureData);
    DataValue->pContext = this;
    DataValue->Type = Input.Type;
    DataValue->Flags = Input.Flags;
    ::AccelerationStructureDesc                Desc = { .Type = Input.Type, .Flags = Input.Flags };
    Array<::AccelerationStructureGeometryDesc> Geometries;
    Array<::AccelerationStructureInstanceDesc> Instances;
    if (Input.Type == ACCELERATION_STRUCTURE_TYPE_BOTTOM)
    {
        ASSERT(Input.geometries.Count);
        for (const AccelerationStructureGeometry& GeometryValue : Input.geometries)
        {
            ASSERT(GeometryValue.pVertexBuffer && GeometryValue.pVertexBuffer->IsValid() && GeometryValue.vertexCount);
            Geometries.PushBack({ .pVertexBuffer = GeometryValue.pVertexBuffer->pBuffer,
                                  .pIndexBuffer = GeometryValue.pIndexBuffer ? GeometryValue.pIndexBuffer->pBuffer : nullptr,
                                  .vertexOffset = GeometryValue.vertexOffset,
                                  .vertexCount = GeometryValue.vertexCount,
                                  .vertexStride = GeometryValue.vertexStride,
                                  .vertexFormat = GeometryValue.vertexFormat,
                                  .indexOffset = GeometryValue.indexOffset,
                                  .indexCount = GeometryValue.indexCount,
                                  .indexType = GeometryValue.indexType,
                                  .Flags = GeometryValue.opaque ? ACCELERATION_STRUCTURE_GEOMETRY_FLAG_OPAQUE
                                                                : ACCELERATION_STRUCTURE_GEOMETRY_FLAG_NO_DUPLICATE_ANYHIT_INVOCATION });
            DataValue->inputs.PushBack(GeometryValue.pVertexBuffer);
            if (GeometryValue.pIndexBuffer)
            {
                DataValue->inputs.PushBack(GeometryValue.pIndexBuffer);
            }
        }
        Desc.bottom = { .descCount = Geometries.Size(), .pGeometryDescs = Geometries.GetData() };
    }
    else
    {
        DataValue->Capacity = TF_MAX(Input.instances.Count, Input.instanceCount);
        ASSERT(DataValue->Capacity);
        DataValue->SetInstances(Input.instances);
        if (!Input.instances.Count)
        {
            DataValue->instances.Resize(DataValue->Capacity);
            DataValue->bottoms.Resize(DataValue->Capacity);
        }
        for (const AccelerationStructureInstance& Instance : Input.instances)
        {
            ASSERT(Instance.pBottom && Instance.pBottom->IsValid());
            ::AccelerationStructureInstanceDesc Native = { .pBottomAS = Instance.pBottom->Data->pStructure,
                                                           .instanceID = Instance.instanceId,
                                                           .instanceMask = Instance.mask,
                                                           .instanceContributionToHitGroupIndex = Instance.hitGroupOffset,
                                                           .Flags = Instance.Flags };
            memcpy(Native.transform, Instance.transform, sizeof(Native.transform));
            Instances.PushBack(Native);
        }
        ASSERT(!Input.instances.Count || Input.instanceCount <= Input.instances.Count);
        Desc.top = { .descCount = DataValue->Capacity, .pInstanceDescs = Instances.GetData() };
    }
    addAccelerationStructure(pRaytracing, &Desc, &DataValue->pStructure);
    if (!DataValue->pStructure)
    {
        for (AccelerationStructureData* Bottom : DataValue->bottoms)
        {
            if (Bottom)
            {
                ReleaseAccelerationStructure(Bottom);
            }
        }
        MemDelete(DataValue);
        return {};
    }
    ++resourceCount;
    return GPUAccelerationStructure(DataValue);
}

void CommandList::BuildAccelerationStructure(GPUAccelerationStructure& Structure, Span<AccelerationStructureInstance> Instances,
                                             bool Update)
{
    ASSERT(Structure.IsValid());
    AccelerationStructureData& DataValue = *Structure.Data;
    ASSERT(DataValue.pContext == pContext);
    ASSERT(!Update || (DataValue.built && (DataValue.Flags & ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE)));
    RaytracingBuildASDesc Desc = { .pAccelerationStructure = DataValue.pStructure, .issueRWBarrier = true, .Update = Update };
    if (DataValue.Type == ACCELERATION_STRUCTURE_TYPE_BOTTOM)
    {
        arrsetlen(bufferBarriers, 0);
        AddBufferBarriers({ DataValue.inputs.GetData(), DataValue.inputs.Size() }, false);
        Barrier((uint32_t)arrlen(bufferBarriers), bufferBarriers, 0, nullptr, 0, nullptr);
    }
    else
    {
        if (Instances.Count)
        {
            DataValue.SetInstances(Instances);
        }
        ASSERT(DataValue.instances.Size() <= DataValue.Capacity && (!Update || DataValue.instances.Size() == DataValue.builtCount));
        for (AccelerationStructureData* Bottom : DataValue.bottoms)
        {
            ASSERT(Bottom && Bottom->built);
        }
        Buffer* Upload = CreateUploadBuffer((uint64_t)DataValue.instances.Size() * sizeof(RaytracingInstanceData));
        memcpy(Upload->pCpuMappedAddress, DataValue.instances.GetData(), DataValue.instances.Size() * sizeof(RaytracingInstanceData));
        Desc.pInstanceBuffer = Upload;
        Desc.instanceCount = DataValue.instances.Size();
        DataValue.builtCount = DataValue.instances.Size();
    }
    cmdBuildAccelerationStructure(pCmd, pContext->pRaytracing, &Desc);
    DataValue.built = true;
    DataValue.dirty = false;
}

void CommandList::UpdateAccelerationStructure(GPUAccelerationStructure& Structure)
{
    ASSERT(Structure.IsValid() && Structure.Data->pContext == pContext);
    AccelerationStructureData& DataValue = *Structure.Data;
    ASSERT(DataValue.Type == ACCELERATION_STRUCTURE_TYPE_TOP);
    if (DataValue.dirty)
    {
        const bool Update = DataValue.built && (DataValue.Flags & ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE) &&
                            DataValue.instances.Size() == DataValue.builtCount;
        BuildAccelerationStructure(Structure, {}, Update);
    }
}

GPURayTracingPipeline::~GPURayTracingPipeline() { Destroy(); }
GPURayTracingPipeline::GPURayTracingPipeline(GPURayTracingPipeline&& Other) noexcept: pContext(Other.pContext), pPipeline(Other.pPipeline)
{
    Other.pContext = nullptr;
    Other.pPipeline = nullptr;
}
GPURayTracingPipeline& GPURayTracingPipeline::operator=(GPURayTracingPipeline&& Other) noexcept
{
    if (this != &Other)
    {
        Destroy();
        pContext = Other.pContext;
        pPipeline = Other.pPipeline;
        Other.pContext = nullptr;
        Other.pPipeline = nullptr;
    }
    return *this;
}
void GPURayTracingPipeline::Destroy()
{
    if (pPipeline)
    {
        pContext->DeferResource(RenderContext::ResourceType::RayTracingPipeline, pPipeline);
        pPipeline = nullptr;
        pContext = nullptr;
    }
}

struct RayEntry
{
    const char* name;
    SlangStage  stage;
};
static void AddRayEntry(Array<RayEntry>& Entries, const char* NameValue, SlangStage Stage)
{
    if (!NameValue)
    {
        return;
    }
    for (const RayEntry& Entry : Entries)
    {
        if (!strcmp(Entry.name, NameValue))
        {
            ASSERT(Entry.stage == Stage);
            return;
        }
    }
    Entries.PushBack({ .name = NameValue, .stage = Stage });
}
static bool RaySlangResult(SlangResult Result, const Slang::ComPtr<ISlangBlob>& Diagnostics)
{
    if (Diagnostics && Diagnostics->getBufferSize())
    {
        LOGF(SLANG_FAILED(Result) ? Error : Warning, "Slang ray tracing: %s", (const char*)Diagnostics->getBufferPointer());
    }
    return SLANG_SUCCEEDED(Result);
}

GPURayTracingPipeline RenderContext::CreateRayTracingPipeline(const RayTracingPipelineDesc& Input)
{
    ASSERT(Input.pFileName && Input.pRayGeneration && Input.rootConstantSize && Input.rootConstantSize <= 256 &&
           !(Input.rootConstantSize % 4));
    if (!pRaytracing && !initRaytracing(pRenderer, &pRaytracing))
    {
        return {};
    }
    slang::IGlobalSession* SlangSession = GetSlangGlobalSession();
    if (!SlangSession)
    {
        return {};
    }
    FileStream Stream = {};
    if (!OpenStreamFromPath(Input.sourceDirectory, Input.pFileName, FileModeRead, &Stream))
    {
        LOGF(Error, "Cannot open ray tracing shader '%s'", Input.pFileName);
        return {};
    }
    const ssize_t Size = GetStreamFileSize(&Stream);
    Array<char>   Source(Size > 0 && Size < UINT32_MAX ? (uint32_t)Size + 1 : 0);
    const bool    Read = Size > 0 && Size < UINT32_MAX && ReadFromStream(&Stream, Source.GetData(), (size_t)Size) == (size_t)Size;
    CloseStream(&Stream);
    if (!Read)
    {
        return {};
    }
    Source[(uint32_t)Size] = 0;
    char        Path[FS_MAX_PATH];
    const char* searchPath = GetResourceDirectory(Input.sourceDirectory);
    if (!AppendPathComponent(searchPath, Input.pFileName, Path))
    {
        return {};
    }
    const slang::CompilerOptionEntry Options[] = {
        { .name = slang::CompilerOptionName::NoMangle, .value = { .intValue0 = 1 } },
    };
    const slang::TargetDesc        Target = { .format = SLANG_DXIL,
                                              .profile = SlangSession->findProfile("sm_6_6"),
                                              .flags = SLANG_TARGET_FLAG_GENERATE_WHOLE_PROGRAM };
    const slang::SessionDesc       SessionDescValue = { .targets = &Target,
                                                        .targetCount = 1,
                                                        .defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR,
                                                        .searchPaths = &searchPath,
                                                        .searchPathCount = 1,
                                                        .compilerOptionEntries = Options,
                                                        .compilerOptionEntryCount = 1 };
    Slang::ComPtr<slang::ISession> Session;
    Slang::ComPtr<ISlangBlob>      Diagnostics;
    if (SLANG_FAILED(SlangSession->createSession(SessionDescValue, Session.writeRef())))
    {
        return {};
    }
    slang::IModule* Module = Session->loadModuleFromSourceString("HorizonRayTracing", Path, Source.GetData(), Diagnostics.writeRef());
    if (!RaySlangResult(Module ? SLANG_OK : SLANG_FAIL, Diagnostics))
    {
        return {};
    }
    Array<RayEntry> Entries;
    AddRayEntry(Entries, Input.pRayGeneration, SLANG_STAGE_RAY_GENERATION);
    for (const char* NameValue : Input.missShaders)
    {
        AddRayEntry(Entries, NameValue, SLANG_STAGE_MISS);
    }
    Array<RaytracingHitGroupDesc> Groups;
    for (const RayTracingHitGroup& Group : Input.hitGroups)
    {
        AddRayEntry(Entries, Group.pClosestHit, SLANG_STAGE_CLOSEST_HIT);
        AddRayEntry(Entries, Group.pAnyHit, SLANG_STAGE_ANY_HIT);
        AddRayEntry(Entries, Group.pIntersection, SLANG_STAGE_INTERSECTION);
        Groups.PushBack(
            { .Name = Group.Name, .pClosestHit = Group.pClosestHit, .pAnyHit = Group.pAnyHit, .pIntersection = Group.pIntersection });
    }
    Array<Slang::ComPtr<slang::IEntryPoint>> EntryPoints(Entries.Size());
    Array<slang::IComponentType*>            Components;
    Components.PushBack(Module);
    for (uint32_t I = 0; I < Entries.Size(); ++I)
    {
        if (!RaySlangResult(
                Module->findAndCheckEntryPoint(Entries[I].name, Entries[I].stage, EntryPoints[I].writeRef(), Diagnostics.writeRef()),
                Diagnostics))
        {
            return {};
        }
        Components.PushBack(EntryPoints[I].get());
    }
    Slang::ComPtr<slang::IComponentType> Program, Linked;
    Slang::ComPtr<ISlangBlob>            Library;
    if (!RaySlangResult(
            Session->createCompositeComponentType(Components.GetData(), Components.Size(), Program.writeRef(), Diagnostics.writeRef()),
            Diagnostics) ||
        !RaySlangResult(Program->link(Linked.writeRef(), Diagnostics.writeRef()), Diagnostics) ||
        !RaySlangResult(Linked->getTargetCode(0, Library.writeRef(), Diagnostics.writeRef()), Diagnostics))
    {
        return {};
    }
    const RaytracingPipelineDesc Desc = { .pLibrary = Library->getBufferPointer(),
                                          .librarySize = Library->getBufferSize(),
                                          .pRayGeneration = Input.pRayGeneration,
                                          .ppMissShaders = Input.missShaders.Data,
                                          .missShaderCount = Input.missShaders.Count,
                                          .pHitGroups = Groups.GetData(),
                                          .hitGroupCount = Groups.Size(),
                                          .payloadSize = Input.payloadSize,
                                          .attributeSize = Input.attributeSize,
                                          .maxRecursionDepth = Input.maxRecursionDepth,
                                          .rootConstantSize = Input.rootConstantSize };
    RaytracingPipeline*          PipelineObject = nullptr;
    addRaytracingPipeline(pRaytracing, &Desc, &PipelineObject);
    if (!PipelineObject)
    {
        return {};
    }
    ++resourceCount;
    return GPURayTracingPipeline(this, PipelineObject);
}

void CommandList::SetPipeline(const GPURayTracingPipeline& PipelineObject)
{
    ASSERT(PipelineObject.IsValid() && PipelineObject.pContext == pContext);
    pCurrentRootSignature = getRaytracingPipelineRootSignature(PipelineObject.pPipeline);
    pCurrentDrawIndirectSignature = nullptr;
    cmdBindRaytracingPipeline(pCmd, PipelineObject.pPipeline);
}
void CommandList::TraceRays(const GPURayTracingPipeline& PipelineObject, uint32_t Width, uint32_t Height, uint32_t Depth,
                            const Dependencies& DependenciesValue)
{
    ASSERT(PipelineObject.IsValid() && Width && Height && Depth);
    Barrier(DependenciesValue, nullptr);
    cmdTraceRays(pCmd, PipelineObject.pPipeline, Width, Height, Depth);
}
} // namespace hz
