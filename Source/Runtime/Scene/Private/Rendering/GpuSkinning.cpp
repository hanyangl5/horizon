/* Copyright (c) 2026 Horizon */

#include "Scene/GpuSkinning.h"
#include "Scene/AssetManager.h"
#include "Scene/MeshAsset.h"
#include "Profiler/IProfiler.h"

#include "Core/IMemory.h"

namespace hz
{
static_assert(sizeof(GpuSkinningJob) == 12 * sizeof(uint32_t));

// Skinned slot of a render instance index.
struct SkinnedSlot
{
    uint32_t key;
    uint32_t value;
};

class GpuSkinningData
{
public:
    GpuSkinningData(RenderScene& Scene, AssetManager& AssetManager, RenderContext& Context, const GpuSkinningDesc& Desc):
        Scene(Scene), AssetManager(AssetManager), Context(Context), Desc(Desc)
    {
        ASSERT(Desc.FrameCount);
        Palettes.Resize(Desc.FrameCount);
        JobBuffers.Resize(Desc.FrameCount);
    }
    ~GpuSkinningData()
    {
        for (GpuSkinnedVertices& Vertices : Skinned)
        {
            Retire(Vertices);
        }
        for (GPUBuffer& Buffer : Palettes)
        {
            Context.DeferRelease(std::move(Buffer));
        }
        for (GPUBuffer& Buffer : JobBuffers)
        {
            Context.DeferRelease(std::move(Buffer));
        }
        hmfree(Slots);
    }

    GpuSkinnedVertices* Find(RenderInstanceID Instance)
    {
        const SkinnedSlot* Slot = hmgetp_null(Slots, Instance.index);
        return Slot && Skinned[Slot->value].Instance == Instance ? &Skinned[Slot->value] : nullptr;
    }

    // Buffers may still be read by submitted frames.
    void Retire(GpuSkinnedVertices& Vertices)
    {
        Context.DeferRelease(std::move(Vertices.Positions));
        Context.DeferRelease(std::move(Vertices.Normals));
        if (Vertices.Texcoords.IsValid())
        {
            Context.DeferRelease(std::move(Vertices.Texcoords));
        }
    }

    GPUBuffer CreateStream(uint32_t Stride, uint32_t VertexCount, const char* Name)
    {
        const uint64_t Size = (uint64_t)Stride * VertexCount;
        GPUBuffer      Buffer =
            Context.CreateBuffer({ .Size = Size,
                                   .ElementCount = (uint32_t)(Size / sizeof(uint32_t)),
                                   .Name = Name,
                                   .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
                                   .StartState = RESOURCE_STATE_UNORDERED_ACCESS,
                                   .Descriptors = (DescriptorType)(DESCRIPTOR_TYPE_BUFFER_RAW | DESCRIPTOR_TYPE_RW_BUFFER_RAW) });
        ASSERT(Buffer.IsValid());
        return Buffer;
    }

    // Upload buffers grow to the next power of two, so a stable scene stops allocating after the first frames.
    void ReserveUpload(GPUBuffer& Buffer, uint64_t Size, const char* Name)
    {
        if (Buffer.IsValid() && Buffer.GetSize() >= Size)
        {
            return;
        }
        uint64_t Capacity = 256;
        while (Capacity < Size)
        {
            Capacity *= 2;
        }
        if (Buffer.IsValid())
        {
            Context.DeferRelease(std::move(Buffer));
        }
        Buffer = Context.CreateBuffer({ .Size = Capacity,
                                        .ElementCount = (uint32_t)(Capacity / sizeof(uint32_t)),
                                        .Name = Name,
                                        .usage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU,
                                        .StartState = RESOURCE_STATE_SHADER_RESOURCE,
                                        .Descriptors = DESCRIPTOR_TYPE_BUFFER_RAW,
                                        .Flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT });
        ASSERT(Buffer.IsValid());
    }

    static bool HasStream(const MeshAsset& Mesh, uint32_t Binding)
    {
        return Binding < Mesh.GetVertexBufferCount() && Mesh.GetVertexBuffer(Binding).IsValid();
    }

    void Prepare(uint32_t Frame)
    {
        PROFILER_SET_CPU_SCOPE_AUTO_NAMED("GpuSkinning::Prepare");
        ASSERT(Frame < Palettes.Size());
        for (GpuSkinnedVertices& Vertices : Skinned)
        {
            Seen.PushBack(false);
        }
        Jobs.Clear();
        JobMeshes.Clear();
        MaxVertexCount = 0;
        for (const RenderSkin& Skin : Scene.GetSkins())
        {
            const RenderInstance*  Instance = Scene.GetInstance(Skin.instance);
            const RenderPrimitive* Primitive = Instance ? Scene.GetPrimitive(Instance->primitive) : nullptr;
            const MeshAsset*       Mesh =
                Primitive ? AssetManager.GetMesh({ .index = Primitive->mesh.index, .generation = Primitive->mesh.generation }) : nullptr;
            if (!Mesh || !HasStream(*Mesh, Desc.PositionBinding) || !HasStream(*Mesh, Desc.NormalBinding) ||
                !HasStream(*Mesh, Desc.JointBinding) || !HasStream(*Mesh, Desc.WeightBinding))
            {
                continue;
            }
            uint32_t First = UINT32_MAX, Last = 0;
            for (uint32_t Submesh = 0; Submesh < Mesh->GetDescription().submeshes.Count; ++Submesh)
            {
                const MeshVertexRange& Range = Mesh->GetVertexRange(Submesh);
                First = TF_MIN(First, Range.first);
                Last = TF_MAX(Last, Range.first + Range.Count);
            }
            const bool         Texcoords = HasStream(*Mesh, Desc.TexcoordBinding);
            // A slot reused by another instance generation, or a changed mesh, gets new streams.
            const SkinnedSlot* Slot = hmgetp_null(Slots, Skin.instance.index);
            uint32_t           Index = Slot ? Slot->value : Skinned.Size();
            if (Slot &&
                (Skinned[Index].Instance != Skin.instance || Skinned[Index].Mesh != Primitive->mesh || Skinned[Index].VertexCount != Last))
            {
                Retire(Skinned[Index]);
                Skinned[Index] = {};
                Skinned[Index].Instance = Skin.instance;
            }
            if (!Slot)
            {
                Skinned.EmplaceBack().Instance = Skin.instance;
                Seen.PushBack(false);
                hmput(Slots, Skin.instance.index, Index);
            }
            GpuSkinnedVertices& Vertices = Skinned[Index];
            Seen[Index] = true;
            if (!Vertices.Positions.IsValid())
            {
                Vertices.Mesh = Primitive->mesh;
                Vertices.VertexCount = Last;
                Vertices.Positions =
                    CreateStream(Mesh->GetVertexStride(Desc.PositionBinding), Vertices.VertexCount, "GpuSkinning.Positions");
                Vertices.Normals = CreateStream(Mesh->GetVertexStride(Desc.NormalBinding), Vertices.VertexCount, "GpuSkinning.Normals");
                if (Texcoords)
                {
                    Vertices.Texcoords =
                        CreateStream(Mesh->GetVertexStride(Desc.TexcoordBinding), Vertices.VertexCount, "GpuSkinning.Texcoords");
                }
            }
            Jobs.PushBack({ .Positions = Mesh->GetVertexBuffer(Desc.PositionBinding).GetSrvIndex(),
                            .Normals = Mesh->GetVertexBuffer(Desc.NormalBinding).GetSrvIndex(),
                            .Texcoords = Texcoords ? Mesh->GetVertexBuffer(Desc.TexcoordBinding).GetSrvIndex() : UINT32_MAX,
                            .Joints = Mesh->GetVertexBuffer(Desc.JointBinding).GetSrvIndex(),
                            .Weights = Mesh->GetVertexBuffer(Desc.WeightBinding).GetSrvIndex(),
                            .OutputPositions = Vertices.Positions.GetUavIndex(),
                            .OutputNormals = Vertices.Normals.GetUavIndex(),
                            .OutputTexcoords = Texcoords ? Vertices.Texcoords.GetUavIndex() : UINT32_MAX,
                            .FirstVertex = First,
                            .VertexCount = Last - First,
                            .JointOffset = Skin.jointOffset });
            JobMeshes.PushBack(Mesh);
            MaxVertexCount = TF_MAX(MaxVertexCount, Last - First);
        }
        for (uint32_t I = 0; I < Skinned.Size();)
        {
            if (Seen[I])
            {
                ++I;
                continue;
            }
            Retire(Skinned[I]);
            hmdel(Slots, Skinned[I].Instance.index);
            if (I + 1 < Skinned.Size())
            {
                Skinned[I] = std::move(Skinned[Skinned.Size() - 1]);
                Seen[I] = Seen[Seen.Size() - 1];
                hmput(Slots, Skinned[I].Instance.index, I);
            }
            Skinned.PopBack();
            Seen.PopBack();
        }
        Seen.Clear();
        const Span<Matrix4> Matrices = Scene.GetSkinMatrices();
        if (!Jobs.Empty())
        {
            ReserveUpload(Palettes[Frame], (uint64_t)Matrices.Count * sizeof(Matrix4), "GpuSkinning.Palette");
            ReserveUpload(JobBuffers[Frame], (uint64_t)Jobs.Size() * sizeof(GpuSkinningJob), "GpuSkinning.Jobs");
            const bool Uploaded =
                Context.UpdateBuffer(Palettes[Frame], 0, Matrices.Data, (uint64_t)Matrices.Count * sizeof(Matrix4)) &&
                Context.UpdateBuffer(JobBuffers[Frame], 0, Jobs.GetData(), (uint64_t)Jobs.Size() * sizeof(GpuSkinningJob));
            ASSERT(Uploaded);
        }
        PreparedFrame = Frame;
    }

    Dependencies GetDependencies(uint32_t Frame) const
    {
        ASSERT(Frame == PreparedFrame);
        Reads.Clear();
        Writes.Clear();
        Reads.PushBack(&Palettes[Frame]);
        Reads.PushBack(&JobBuffers[Frame]);
        for (const MeshAsset* Mesh : JobMeshes)
        {
            Reads.PushBack(&Mesh->GetVertexBuffer(Desc.PositionBinding));
            Reads.PushBack(&Mesh->GetVertexBuffer(Desc.NormalBinding));
            Reads.PushBack(&Mesh->GetVertexBuffer(Desc.JointBinding));
            Reads.PushBack(&Mesh->GetVertexBuffer(Desc.WeightBinding));
            if (HasStream(*Mesh, Desc.TexcoordBinding))
            {
                Reads.PushBack(&Mesh->GetVertexBuffer(Desc.TexcoordBinding));
            }
        }
        for (const GpuSkinnedVertices& Vertices : Skinned)
        {
            Writes.PushBack(&Vertices.Positions);
            Writes.PushBack(&Vertices.Normals);
            if (Vertices.Texcoords.IsValid())
            {
                Writes.PushBack(&Vertices.Texcoords);
            }
        }
        return { .buffers = { Reads.GetData(), Reads.Size() }, .storageBuffers = { Writes.GetData(), Writes.Size() } };
    }

    RenderScene&                    Scene;
    AssetManager&                   AssetManager;
    RenderContext&                  Context;
    GpuSkinningDesc                 Desc;
    Array<GPUBuffer>                Palettes;
    Array<GPUBuffer>                JobBuffers;
    Array<GpuSkinnedVertices>       Skinned;
    SkinnedSlot*                    Slots = nullptr;
    Array<bool>                     Seen;
    Array<GpuSkinningJob>           Jobs;
    Array<const MeshAsset*>         JobMeshes;
    mutable Array<const GPUBuffer*> Reads;
    mutable Array<const GPUBuffer*> Writes;
    uint32_t                        MaxVertexCount = 0;
    uint32_t                        PreparedFrame = UINT32_MAX;
};

GpuSkinning::GpuSkinning(RenderScene& Scene, AssetManager& AssetManager, RenderContext& Context, const GpuSkinningDesc& Desc):
    Data(MemNew(GpuSkinningData, Scene, AssetManager, Context, Desc))
{
}
GpuSkinning::~GpuSkinning() { MemDelete(Data); }
void                      GpuSkinning::Prepare(uint32_t Frame) { Data->Prepare(Frame); }
const GPUBuffer&          GpuSkinning::GetPalette(uint32_t Frame) const { return Data->Palettes[Frame]; }
const GPUBuffer&          GpuSkinning::GetJobs(uint32_t Frame) const { return Data->JobBuffers[Frame]; }
uint32_t                  GpuSkinning::GetJobCount() const { return Data->Jobs.Size(); }
uint32_t                  GpuSkinning::GetMaxVertexCount() const { return Data->MaxVertexCount; }
const GpuSkinnedVertices* GpuSkinning::Find(RenderInstanceID Instance) const
{
    const GpuSkinnedVertices* Vertices = Data->Find(Instance);
    return Vertices && Vertices->Positions.IsValid() ? Vertices : nullptr;
}
Span<GpuSkinnedVertices> GpuSkinning::GetSkinnedVertices() const { return { Data->Skinned.GetData(), Data->Skinned.Size() }; }
Dependencies             GpuSkinning::GetDependencies(uint32_t Frame) const { return Data->GetDependencies(Frame); }
} // namespace hz
