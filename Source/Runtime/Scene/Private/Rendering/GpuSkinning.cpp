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
    GpuSkinningData(RenderScene& scene, AssetManager& assetManager, RenderContext& context, const GpuSkinningDesc& desc):
        scene(scene), assetManager(assetManager), context(context), desc(desc)
    {
        ASSERT(desc.frameCount);
        palettes.Resize(desc.frameCount);
        jobBuffers.Resize(desc.frameCount);
    }
    ~GpuSkinningData()
    {
        for (GpuSkinnedVertices& vertices : skinned)
            retire(vertices);
        for (GPUBuffer& buffer : palettes)
            context.DeferRelease(std::move(buffer));
        for (GPUBuffer& buffer : jobBuffers)
            context.DeferRelease(std::move(buffer));
        hmfree(slots);
    }

    GpuSkinnedVertices* find(RenderInstanceID instance)
    {
        const SkinnedSlot* slot = hmgetp_null(slots, instance.index);
        return slot && skinned[slot->value].instance == instance ? &skinned[slot->value] : nullptr;
    }

    // Buffers may still be read by submitted frames.
    void retire(GpuSkinnedVertices& vertices)
    {
        context.DeferRelease(std::move(vertices.positions));
        context.DeferRelease(std::move(vertices.normals));
        if (vertices.texcoords.IsValid())
            context.DeferRelease(std::move(vertices.texcoords));
    }

    GPUBuffer createStream(uint32_t stride, uint32_t vertexCount, const char* name)
    {
        const uint64_t size = (uint64_t)stride * vertexCount;
        GPUBuffer      buffer = context.CreateBuffer({ .Size = size,
                                                       .ElementCount = (uint32_t)(size / sizeof(uint32_t)),
                                                       .Name = name,
                                                       .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
                                                       .StartState = RESOURCE_STATE_UNORDERED_ACCESS,
                                                       .Descriptors = (DescriptorType)(DESCRIPTOR_TYPE_BUFFER_RAW | DESCRIPTOR_TYPE_RW_BUFFER_RAW) });
        ASSERT(buffer.IsValid());
        return buffer;
    }

    // Upload buffers grow to the next power of two, so a stable scene stops allocating after the first frames.
    void reserveUpload(GPUBuffer& buffer, uint64_t size, const char* name)
    {
        if (buffer.IsValid() && buffer.GetSize() >= size)
            return;
        uint64_t capacity = 256;
        while (capacity < size)
            capacity *= 2;
        if (buffer.IsValid())
            context.DeferRelease(std::move(buffer));
        buffer = context.CreateBuffer({ .Size = capacity,
                                        .ElementCount = (uint32_t)(capacity / sizeof(uint32_t)),
                                        .Name = name,
                                        .usage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU,
                                        .StartState = RESOURCE_STATE_SHADER_RESOURCE,
                                        .Descriptors = DESCRIPTOR_TYPE_BUFFER_RAW,
                                        .Flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT });
        ASSERT(buffer.IsValid());
    }

    static bool hasStream(const MeshAsset& mesh, uint32_t binding)
    {
        return binding < mesh.GetVertexBufferCount() && mesh.GetVertexBuffer(binding).IsValid();
    }

    void Prepare(uint32_t frame)
    {
        PROFILER_SET_CPU_SCOPE_AUTO_NAMED("GpuSkinning::Prepare");
        ASSERT(frame < palettes.Size());
        for (GpuSkinnedVertices& vertices : skinned)
            seen.PushBack(false);
        jobs.Clear();
        jobMeshes.Clear();
        maxVertexCount = 0;
        for (const RenderSkin& skin : scene.GetSkins())
        {
            const RenderInstance*  instance = scene.GetInstance(skin.instance);
            const RenderPrimitive* primitive = instance ? scene.GetPrimitive(instance->primitive) : nullptr;
            const MeshAsset* mesh = primitive ? assetManager.GetMesh({ .index = primitive->mesh.index, .generation = primitive->mesh.generation })
                                                : nullptr;
            if (!mesh || !hasStream(*mesh, desc.positionBinding) || !hasStream(*mesh, desc.normalBinding) ||
                !hasStream(*mesh, desc.jointBinding) || !hasStream(*mesh, desc.weightBinding))
            {
                continue;
            }
            uint32_t first = UINT32_MAX, last = 0;
            for (uint32_t submesh = 0; submesh < mesh->GetDescription().submeshes.Count; ++submesh)
            {
                const MeshVertexRange& range = mesh->GetVertexRange(submesh);
                first = TF_MIN(first, range.first);
                last = TF_MAX(last, range.first + range.Count);
            }
            const bool texcoords = hasStream(*mesh, desc.texcoordBinding);
            // A slot reused by another instance generation, or a changed mesh, gets new streams.
            const SkinnedSlot* slot = hmgetp_null(slots, skin.instance.index);
            uint32_t           index = slot ? slot->value : skinned.Size();
            if (slot && (skinned[index].instance != skin.instance || skinned[index].mesh != primitive->mesh ||
                         skinned[index].vertexCount != last))
            {
                retire(skinned[index]);
                skinned[index] = {};
                skinned[index].instance = skin.instance;
            }
            if (!slot)
            {
                skinned.EmplaceBack().instance = skin.instance;
                seen.PushBack(false);
                hmput(slots, skin.instance.index, index);
            }
            GpuSkinnedVertices& vertices = skinned[index];
            seen[index] = true;
            if (!vertices.positions.IsValid())
            {
                vertices.mesh = primitive->mesh;
                vertices.vertexCount = last;
                vertices.positions = createStream(mesh->GetVertexStride(desc.positionBinding), vertices.vertexCount, "GpuSkinning.Positions");
                vertices.normals = createStream(mesh->GetVertexStride(desc.normalBinding), vertices.vertexCount, "GpuSkinning.Normals");
                if (texcoords)
                    vertices.texcoords =
                        createStream(mesh->GetVertexStride(desc.texcoordBinding), vertices.vertexCount, "GpuSkinning.Texcoords");
            }
            jobs.PushBack({ .positions = mesh->GetVertexBuffer(desc.positionBinding).GetSrvIndex(),
                            .normals = mesh->GetVertexBuffer(desc.normalBinding).GetSrvIndex(),
                            .texcoords = texcoords ? mesh->GetVertexBuffer(desc.texcoordBinding).GetSrvIndex() : UINT32_MAX,
                            .joints = mesh->GetVertexBuffer(desc.jointBinding).GetSrvIndex(),
                            .weights = mesh->GetVertexBuffer(desc.weightBinding).GetSrvIndex(),
                            .outputPositions = vertices.positions.GetUavIndex(),
                            .outputNormals = vertices.normals.GetUavIndex(),
                            .outputTexcoords = texcoords ? vertices.texcoords.GetUavIndex() : UINT32_MAX,
                            .firstVertex = first,
                            .vertexCount = last - first,
                            .jointOffset = skin.jointOffset });
            jobMeshes.PushBack(mesh);
            maxVertexCount = TF_MAX(maxVertexCount, last - first);
        }
        for (uint32_t i = 0; i < skinned.Size();)
        {
            if (seen[i])
            {
                ++i;
                continue;
            }
            retire(skinned[i]);
            hmdel(slots, skinned[i].instance.index);
            if (i + 1 < skinned.Size())
            {
                skinned[i] = std::move(skinned[skinned.Size() - 1]);
                seen[i] = seen[seen.Size() - 1];
                hmput(slots, skinned[i].instance.index, i);
            }
            skinned.PopBack();
            seen.PopBack();
        }
        seen.Clear();
        const Span<Matrix4> matrices = scene.GetSkinMatrices();
        if (!jobs.Empty())
        {
            reserveUpload(palettes[frame], (uint64_t)matrices.Count * sizeof(Matrix4), "GpuSkinning.Palette");
            reserveUpload(jobBuffers[frame], (uint64_t)jobs.Size() * sizeof(GpuSkinningJob), "GpuSkinning.Jobs");
            const bool uploaded = context.UpdateBuffer(palettes[frame], 0, matrices.Data, (uint64_t)matrices.Count * sizeof(Matrix4)) &&
                                  context.UpdateBuffer(jobBuffers[frame], 0, jobs.GetData(), (uint64_t)jobs.Size() * sizeof(GpuSkinningJob));
            ASSERT(uploaded);
        }
        preparedFrame = frame;
    }

    Dependencies GetDependencies(uint32_t frame) const
    {
        ASSERT(frame == preparedFrame);
        reads.Clear();
        writes.Clear();
        reads.PushBack(&palettes[frame]);
        reads.PushBack(&jobBuffers[frame]);
        for (const MeshAsset* mesh : jobMeshes)
        {
            reads.PushBack(&mesh->GetVertexBuffer(desc.positionBinding));
            reads.PushBack(&mesh->GetVertexBuffer(desc.normalBinding));
            reads.PushBack(&mesh->GetVertexBuffer(desc.jointBinding));
            reads.PushBack(&mesh->GetVertexBuffer(desc.weightBinding));
            if (hasStream(*mesh, desc.texcoordBinding))
                reads.PushBack(&mesh->GetVertexBuffer(desc.texcoordBinding));
        }
        for (const GpuSkinnedVertices& vertices : skinned)
        {
            writes.PushBack(&vertices.positions);
            writes.PushBack(&vertices.normals);
            if (vertices.texcoords.IsValid())
                writes.PushBack(&vertices.texcoords);
        }
        return { .buffers = { reads.GetData(), reads.Size() }, .storageBuffers = { writes.GetData(), writes.Size() } };
    }

    RenderScene&                    scene;
    AssetManager&                   assetManager;
    RenderContext&                  context;
    GpuSkinningDesc                 desc;
    Array<GPUBuffer>                palettes;
    Array<GPUBuffer>                jobBuffers;
    Array<GpuSkinnedVertices>       skinned;
    SkinnedSlot*                    slots = nullptr;
    Array<bool>                     seen;
    Array<GpuSkinningJob>           jobs;
    Array<const MeshAsset*>         jobMeshes;
    mutable Array<const GPUBuffer*> reads;
    mutable Array<const GPUBuffer*> writes;
    uint32_t                        maxVertexCount = 0;
    uint32_t                        preparedFrame = UINT32_MAX;
};

GpuSkinning::GpuSkinning(RenderScene& scene, AssetManager& assetManager, RenderContext& context, const GpuSkinningDesc& desc):
    Data(MemNew(GpuSkinningData, scene, assetManager, context, desc))
{
}
GpuSkinning::~GpuSkinning() { MemDelete(Data); }
void             GpuSkinning::Prepare(uint32_t frame) { Data->Prepare(frame); }
const GPUBuffer& GpuSkinning::GetPalette(uint32_t frame) const { return Data->palettes[frame]; }
const GPUBuffer& GpuSkinning::GetJobs(uint32_t frame) const { return Data->jobBuffers[frame]; }
uint32_t         GpuSkinning::GetJobCount() const { return Data->jobs.Size(); }
uint32_t         GpuSkinning::GetMaxVertexCount() const { return Data->maxVertexCount; }
const GpuSkinnedVertices* GpuSkinning::Find(RenderInstanceID instance) const
{
    const GpuSkinnedVertices* vertices = Data->find(instance);
    return vertices && vertices->positions.IsValid() ? vertices : nullptr;
}
Span<GpuSkinnedVertices> GpuSkinning::GetSkinnedVertices() const { return { Data->skinned.GetData(), Data->skinned.Size() }; }
Dependencies             GpuSkinning::GetDependencies(uint32_t frame) const { return Data->GetDependencies(frame); }
} // namespace hz
