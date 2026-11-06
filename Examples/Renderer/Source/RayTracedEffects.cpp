#include "RayTracedEffects.h"
#include "Profiler/IProfiler.h"
#include "Scene/MeshAsset.h"
#include "Scene/TextureAsset.h"

static void appendRTBuffer(hz::Array<const hz::GPUBuffer*>& buffers, const hz::GPUBuffer* buffer)
{
    for (const hz::GPUBuffer* existing : buffers)
    {
        if (existing == buffer)
        {
            return;
        }
    }
    buffers.pushBack(buffer);
}

class RayTracingScene
{
public:
    explicit RayTracingScene(hz::RenderContext& context): context(context) {}
    void prepare(hz::CommandList& commands, const SceneView& view)
    {
        ASSERT(view.frameSlot < frames.size());
        Frame&                                        frame = frames[view.frameSlot];
        hz::Array<Bottom*>&                           nextBottoms = frame.nextBottoms;
        hz::Array<hz::AccelerationStructureInstance>& instances = frame.instances;
        nextBottoms.clear();
        instances.clear();
        buffers.clear();
        textures.clear();
        const hz::GpuAssetTables&       tables = view.gpu.getAssetTables();
        uint32_t                        record = 0;
        for (uint32_t slot : view.scene.getActiveMeshBatches())
        {
            const hz::RenderMeshBatch&   draw = view.scene.getMeshBatches().pData[slot];
            const hz::RenderPrimitive&   primitive = view.scene.getPrimitives().pData[draw.primitive.index];
            const hz::GpuMeshRecord*     meshRecord = tables.getMesh(draw.mesh.mesh);
            const hz::GpuMaterialRecord* material = tables.getMaterial(draw.material);
            if (!primitive.instanceCount || !meshRecord || !material)
            {
                continue;
            }
            // Match GpuDrawList exactly, including record IDs consumed by blended draws.
            if (material->alphaMode == uint32_t(hz::MaterialAlphaMode::Blend))
            {
                record += primitive.instanceCount;
                continue;
            }
            const hz::MeshAsset* mesh = view.assets.getMesh({ draw.mesh.mesh.index, draw.mesh.mesh.generation });
            ASSERT(mesh && meshRecord->vertexStreamCount == 3);
            Bottom* bottom = findBottom(draw.mesh, material->alphaMode == uint32_t(hz::MaterialAlphaMode::Opaque));
            if (!bottom)
            {
                hz::unique_ptr<Bottom> candidate = hz::make_unique<Bottom>();
                candidate->mesh = draw.mesh;
                candidate->opaque = material->alphaMode == uint32_t(hz::MaterialAlphaMode::Opaque);
                const IndirectDrawIndexArguments& submesh = mesh->getDraw(draw.mesh.submeshIndex);
                const hz::GPUBuffer&              vertices = mesh->getVertexBuffer(0);
                const uint32_t                    stride = mesh->getVertexStride(0);
                const int32_t                     baseVertex = int32_t(submesh.vertexOffset);
                // Cooked GeometryTF uses nonnegative base vertices and triangle lists.
                ASSERT(baseVertex >= 0 && stride == 12 && submesh.indexCount % 3 == 0);
                const uint64_t vertexOffset = uint64_t(baseVertex) * stride;
                ASSERT(vertices.getSize() >= vertexOffset && (vertices.getSize() - vertexOffset) / stride <= UINT32_MAX);
                const hz::AccelerationStructureGeometry geometry = {
                    .pVertexBuffer = &vertices,
                    .pIndexBuffer = &mesh->getIndexBuffer(),
                    .vertexOffset = vertexOffset,
                    .indexOffset = uint64_t(submesh.startIndex) * (mesh->getIndexType() == INDEX_TYPE_UINT16 ? 2u : 4u),
                    .vertexCount = uint32_t((vertices.getSize() - vertexOffset) / stride),
                    .vertexStride = stride,
                    .indexCount = submesh.indexCount,
                    .indexType = mesh->getIndexType(),
                    .opaque = candidate->opaque,
                };
                candidate->structure = context.createAccelerationStructure({ .geometries = { &geometry, 1 } });
                ASSERT(candidate->structure.isValid());
                commands.buildAccelerationStructure(candidate->structure);
                bottom = candidate.get();
                bottoms.pushBack(std::move(candidate));
            }
            for (uint32_t stream = 0; stream < mesh->getVertexBufferCount(); ++stream)
            {
                appendRTBuffer(buffers, &mesh->getVertexBuffer(stream));
            }
            appendRTBuffer(buffers, &mesh->getIndexBuffer());
            for (uint32_t i = 0; i < primitive.instanceCount; ++i, ++record)
            {
                const uint32_t                    instanceIndex = view.scene.getInstanceIndices().pData[primitive.instanceOffset + i];
                const Matrix4&                    transform = view.scene.getInstances().pData[instanceIndex].current;
                hz::AccelerationStructureInstance instance = {
                    .pBottom = &bottom->structure,
                    .instanceId = record,
                    .flags = ACCELERATION_STRUCTURE_INSTANCE_FLAG_TRIANGLE_CULL_DISABLE,
                };
                for (uint32_t row = 0; row < 3; ++row)
                {
                    for (uint32_t column = 0; column < 4; ++column)
                    {
                        instance.transform[row * 4 + column] = transform.getCol(column).getElem(row);
                    }
                }
                instances.pushBack(instance);
                ++bottom->references;
                nextBottoms.pushBack(bottom);
            }
        }
        for (const hz::GpuTextureRecord& texture : tables.textures)
        {
            if (texture.id.isValid())
            {
                const hz::TextureAsset* asset = view.assets.getTexture({ texture.id.index, texture.id.generation });
                ASSERT(asset);
                textures.pushBack(&asset->getTexture());
            }
        }
        const uint32_t count = instances.size();
        if (frame.haveInstancesChanged())
        {
            const bool update = frame.structure.isValid() && frame.count == count;
            if (count && !update)
            {
                frame.structure = context.createAccelerationStructure({
                    .type = ACCELERATION_STRUCTURE_TYPE_TOP,
                    .flags = AccelerationStructureBuildFlags(ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE |
                                                             ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE),
                    .instances = { instances.data(), count },
                });
            }
            if (count)
            {
                ASSERT(frame.structure.isValid());
                commands.buildAccelerationStructure(frame.structure, { instances.data(), count }, update);
            }
            else
            {
                frame.structure = {};
            }
            frame.builtInstances.resize(count);
            for (uint32_t i = 0; i < count; ++i)
            {
                frame.builtInstances[i] = instances[i];
            }
            frame.count = count;
        }
        // This backing is no longer in flight. Other backings retain every BLAS they reference.
        for (Bottom* bottom : frame.bottoms)
        {
            --bottom->references;
        }
        hz::Array<Bottom*> previousBottoms = std::move(frame.bottoms);
        frame.bottoms = std::move(nextBottoms);
        nextBottoms = std::move(previousBottoms);
        nextBottoms.clear();
        for (uint32_t i = 0; i < bottoms.size();)
        {
            if (bottoms[i]->references)
            {
                ++i;
            }
            else
            {
                bottoms[i] = std::move(bottoms[bottoms.size() - 1]);
                bottoms.popBack();
            }
        }
    }
    const hz::GPUAccelerationStructure& getTop(uint32_t backing) const { return frames[backing].structure; }
    hz::Array<const hz::GPUBuffer*>     buffers;
    hz::Array<const hz::GPUTexture*>    textures;

private:
    struct Bottom
    {
        hz::GpuSubmeshRef            mesh;
        hz::GPUAccelerationStructure structure;
        uint32_t                     references = 0;
        bool                         opaque = true;
    };
    struct Frame
    {
        hz::GPUAccelerationStructure                 structure;
        hz::Array<Bottom*>                           bottoms;
        hz::Array<Bottom*>                           nextBottoms;
        hz::Array<hz::AccelerationStructureInstance> instances;
        hz::Array<hz::AccelerationStructureInstance> builtInstances;
        uint32_t                                     count = 0;

        bool haveInstancesChanged() const
        {
            if (builtInstances.size() != instances.size() || (!instances.empty() && !structure.isValid()))
            {
                return true;
            }
            for (uint32_t i = 0; i < instances.size(); ++i)
            {
                const hz::AccelerationStructureInstance& previous = builtInstances[i];
                const hz::AccelerationStructureInstance& current = instances[i];
                if (previous.pBottom != current.pBottom || previous.instanceId != current.instanceId || previous.mask != current.mask ||
                    previous.hitGroupOffset != current.hitGroupOffset || previous.flags != current.flags)
                {
                    return true;
                }
                for (uint32_t element = 0; element < 12; ++element)
                {
                    if (previous.transform[element] != current.transform[element])
                    {
                        return true;
                    }
                }
            }
            return false;
        }
    };
    Bottom* findBottom(hz::GpuSubmeshRef mesh, bool opaque) const
    {
        for (const hz::unique_ptr<Bottom>& bottom : bottoms)
        {
            if (bottom->mesh == mesh && bottom->opaque == opaque)
            {
                return bottom.get();
            }
        }
        return nullptr;
    }
    hz::RenderContext&                context;
    // Heap objects keep the AS address stable when this container grows.
    hz::Array<hz::unique_ptr<Bottom>> bottoms;
    hz::FixedArray<Frame, 3>          frames;
};

struct RTConstants
{
    uint32_t frame;
    uint32_t lighting;
    uint32_t draws;
    uint32_t materials;
    uint32_t textures;
    uint32_t sampler;
    uint32_t top;
    uint32_t normal;
    uint32_t depth;
    uint32_t output;
    uint32_t blueNoise;
    uint32_t environment;
    uint32_t irradiance;
    uint32_t brdf;
    uint32_t environmentSampler;
    uint32_t shadows;
    uint32_t ao;
    uint32_t aoSamples;
    uint32_t frameNumber;
    uint32_t alphaMethod;
    uint32_t retraceDepth;
    float    aoRadius;
    float    aoPower;
    float    shadowRadius;
};
struct RTDenoiseConstants
{
    uint32_t frame;
    uint32_t input;
    uint32_t history;
    uint32_t depth;
    uint32_t previousDepth;
    uint32_t output;
    uint32_t sampler;
    uint32_t historyValid;
    float    alpha;
    float    depthThreshold;
    uint32_t horizontal;
    uint32_t step;
};

RayTracedEffects::RayTracedEffects(hz::RenderContext& context): context(context), supported(context.supportsRayTracing())
{
    if (!supported)
    {
        return;
    }
    rayScene = hz::make_unique<RayTracingScene>(context);
    sampler = context.createSampler(
        { .addressU = ADDRESS_MODE_CLAMP_TO_EDGE, .addressV = ADDRESS_MODE_CLAMP_TO_EDGE, .addressW = ADDRESS_MODE_CLAMP_TO_EDGE });
    materialSampler = context.createSampler();
    shadowPipeline = context.createComputePipeline({
        .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "main" } }, .pFileName = "RTShadowAO.slang" },
        .pName = "Renderer.RT.ShadowAO",
    });
    temporalPipeline = context.createComputePipeline({
        .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "main" } },
                        .pFileName = "rt_denoise_temporal.comp.slang" },
        .pName = "Renderer.RT.Temporal",
    });
    spatialPipeline = context.createComputePipeline({
        .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "main" } }, .pFileName = "rt_denoise_spatial.comp.slang" },
        .pName = "Renderer.RT.Spatial",
    });
    atrousPipeline = context.createComputePipeline({
        .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "main" } }, .pFileName = "rt_denoise_atrous.comp.slang" },
        .pName = "Renderer.RT.Atrous",
    });
    depthCopyPipeline = context.createComputePipeline({
        .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "CSCopyDepth" } }, .pFileName = "RTDenoise.slang" },
        .pName = "Renderer.RT.DepthHistory",
    });
    reflectionPipeline = context.createRayTracingPipeline({
        .pFileName = "RTReflection.slang",
        .pRayGeneration = "rayGenMain",
        .missShaders = { "missMain" },
        .hitGroups = { { .pName = "ReflectionHit", .pClosestHit = "closestHitMain" } },
        .payloadSize = 16,
        .maxRecursionDepth = 2,
        .pName = "Renderer.RT.Reflection",
    });
    ASSERT(sampler.isValid() && materialSampler.isValid() && shadowPipeline.isValid() && temporalPipeline.isValid() &&
           spatialPipeline.isValid() && atrousPipeline.isValid() && depthCopyPipeline.isValid() && reflectionPipeline.isValid());
}

RayTracedEffects::~RayTracedEffects() = default;

void RayTracedEffects::resize(uint32_t width, uint32_t height)
{
    if (!supported)
    {
        return;
    }
    ASSERT(width && height);
    this->width = width;
    this->height = height;
    const DescriptorType views = DescriptorType(DESCRIPTOR_TYPE_TEXTURE | DESCRIPTOR_TYPE_RW_TEXTURE);
    hz::TextureDesc      desc = { .width = width,
                                  .height = height,
                                  .format = hz::Format::R8G8_UNORM,
                                  .startState = RESOURCE_STATE_UNORDERED_ACCESS,
                                  .descriptors = views,
                                  .flags = TEXTURE_CREATION_FLAG_FORCE_2D,
                                  .pName = "Renderer.RT.ShadowAO" };
    result = context.createTexture(desc);
    desc.pName = "Renderer.RT.DenoiseScratch";
    scratch = context.createTexture(desc);
    desc.pName = "Renderer.RT.History";
    for (hz::GPUTexture& texture : history)
    {
        texture = context.createTexture(desc);
    }
    desc.format = hz::Format::R32_SFLOAT;
    desc.pName = "Renderer.RT.PreviousDepth";
    previousDepth = context.createTexture(desc);
    desc.format = hz::Format::R32G32B32A32_SFLOAT;
    desc.pName = "Renderer.RT.Reflection";
    reflection = context.createTexture(desc);
    if (!blueNoise.isValid())
    {
        blueNoise = context.loadTexture(RD_SHADER_SOURCES, "../Assets/blue_noise_128x128x64.dds");
    }
    historyIndex = 0;
    resetHistory();
    ASSERT(result.isValid() && scratch.isValid() && history[0].isValid() && history[1].isValid() && previousDepth.isValid() &&
           reflection.isValid() && blueNoise.isValid() && blueNoise.getWidth() == 128 && blueNoise.getHeight() == 8192);
}

void RayTracedEffects::resetHistory() { historyValid = false; }

void RayTracedEffects::execute(hz::CommandList& commands, const hz::GPUBuffer& frame, const DeferredGBuffer& gbuffer, const SceneView& view,
                               const GpuDrawList& draws, const LightingResources& lighting)
{
    PROFILER_SET_CPU_SCOPE_AUTO_NAMED("RayTracedEffects::execute");
    if (!supported)
    {
        return;
    }
    ASSERT(width && height && gbuffer.pNormalMaterial && gbuffer.pDepth && lighting.pLightingFrame);
    if (!(settings == previousSettings))
    {
        resetHistory();
    }
    previousSettings = settings;
    if (assetRevision != view.gpu.getAssetTables().revision)
    {
        resetHistory();
    }
    assetRevision = view.gpu.getAssetTables().revision;
    commands.beginGpuTimestamp("RT Acceleration Structures");
    rayScene->prepare(commands, view);
    commands.endGpuTimestamp();
    const hz::GPUAccelerationStructure& top = rayScene->getTop(view.frameSlot);
    const hz::GPUBuffer&                records = draws.getRecords(view.frameSlot);
    auto        index = [](const hz::GPUTexture* texture) { return texture && texture->isValid() ? texture->getSrvIndex() : UINT32_MAX; };
    RTConstants                         constants = {
                                .frame = frame.getSrvIndex(),
                                .lighting = lighting.pLightingFrame->getSrvIndex(),
                                .draws = records.getSrvIndex(),
                                .materials = view.gpu.getBuffers().materials.getSrvIndex(),
                                .textures = view.gpu.getBuffers().textures.getSrvIndex(),
                                .sampler = materialSampler.getIndex(),
                                .top = top.isValid() ? top.getSrvIndex() : UINT32_MAX,
                                .normal = gbuffer.pNormalMaterial->getSrvIndex(),
                                .depth = gbuffer.pDepth->getSrvIndex(),
                                .output = result.getUavIndex(),
                                .blueNoise = blueNoise.getSrvIndex(),
                                .environment = index(lighting.pEnvironment),
                                .irradiance = index(lighting.pIrradiance),
                                .brdf = index(lighting.pBRDFLut),
                                .environmentSampler = lighting.pEnvironmentSampler ? lighting.pEnvironmentSampler->getIndex() : UINT32_MAX,
                                .shadows = settings.shadows,
                                .ao = settings.ao,
                                .aoSamples = TF_MAX(settings.aoSamples, 1u),
                                .frameNumber = frameIndex++,
                                .alphaMethod = settings.shadowAlphaTestMethod,
                                .retraceDepth = settings.retraceMaxDepth,
                                .aoRadius = settings.aoRadius,
                                .aoPower = settings.aoPower,
                                .shadowRadius = settings.shadowRadius,
    };
    hz::Array<const hz::GPUBuffer*>& buffers = rayScene->buffers;
    appendRTBuffer(buffers, &frame);
    appendRTBuffer(buffers, lighting.pLightingFrame);
    appendRTBuffer(buffers, &records);
    appendRTBuffer(buffers, &view.gpu.getBuffers().materials);
    appendRTBuffer(buffers, &view.gpu.getBuffers().textures);
    hz::Array<const hz::GPUTexture*>& textures = rayScene->textures;
    textures.pushBack(gbuffer.pNormalMaterial);
    textures.pushBack(gbuffer.pDepth);
    textures.pushBack(&blueNoise);
    commands.beginGpuTimestamp("RT Shadow AO");
    commands.setPipeline(shadowPipeline);
    commands.setRootConstant({ reinterpret_cast<const uint32_t*>(&constants), sizeof(constants) / 4 });
    commands.dispatch((width + 15) / 16, (height + 15) / 16, 1,
                      { .sampledTextures = { textures.data(), textures.size() },
                        .storageTextures = { &result },
                        .buffers = { buffers.data(), buffers.size() } });
    commands.endGpuTimestamp();

    commands.beginGpuTimestamp("RT Denoise");
    const uint32_t     current = historyIndex;
    const uint32_t     previous = 1 - current;
    RTDenoiseConstants denoise = {
        .frame = frame.getSrvIndex(),
        .input = result.getSrvIndex(),
        .history = history[previous].getSrvIndex(),
        .depth = gbuffer.pDepth->getSrvIndex(),
        .previousDepth = previousDepth.getSrvIndex(),
        .output = history[current].getUavIndex(),
        .sampler = sampler.getIndex(),
        .historyValid = historyValid,
        .alpha = settings.temporalAlpha,
        .depthThreshold = settings.temporalDepthThreshold,
    };
    commands.setPipeline(temporalPipeline);
    commands.setRootConstant({ reinterpret_cast<const uint32_t*>(&denoise), sizeof(denoise) / 4 });
    commands.dispatch((width + 15) / 16, (height + 15) / 16, 1,
                      { .sampledTextures = { &result, &history[previous], gbuffer.pDepth, &previousDepth },
                        .storageTextures = { &history[current] },
                        .buffers = { &frame } });
    const bool atrous = settings.denoiseMode == RTDenoiseMode::Atrous;
    denoise.depthThreshold = atrous ? settings.atrousDepthSigma : settings.spatialDepthThreshold;
    for (uint32_t pass = 0; pass < 2; ++pass)
    {
        const hz::GPUTexture& input = pass ? scratch : history[current];
        const hz::GPUTexture& output = pass ? result : scratch;
        denoise.input = input.getSrvIndex();
        denoise.output = output.getUavIndex();
        denoise.horizontal = pass == 0;
        denoise.step = pass + 1;
        commands.setPipeline(atrous ? atrousPipeline : spatialPipeline);
        commands.setRootConstant({ reinterpret_cast<const uint32_t*>(&denoise), sizeof(denoise) / 4 });
        commands.dispatch((width + 15) / 16, (height + 15) / 16, 1,
                          { .sampledTextures = { &input, gbuffer.pDepth }, .storageTextures = { &output } });
    }
    denoise.output = previousDepth.getUavIndex();
    commands.setPipeline(depthCopyPipeline);
    commands.setRootConstant({ reinterpret_cast<const uint32_t*>(&denoise), sizeof(denoise) / 4 });
    commands.dispatch((width + 15) / 16, (height + 15) / 16, 1,
                      { .sampledTextures = { gbuffer.pDepth }, .storageTextures = { &previousDepth } });
    historyIndex = previous;
    historyValid = true;
    commands.endGpuTimestamp();

    commands.beginGpuTimestamp("RT Reflection");
    if (lighting.pEnvironment)
    {
        textures.pushBack(lighting.pEnvironment);
    }
    if (lighting.pIrradiance)
    {
        textures.pushBack(lighting.pIrradiance);
    }
    if (lighting.pBRDFLut)
    {
        textures.pushBack(lighting.pBRDFLut);
    }
    constants.output = reflection.getUavIndex();
    if (!settings.reflection)
    {
        constants.top = UINT32_MAX;
    }
    commands.setPipeline(reflectionPipeline);
    commands.setRootConstant({ reinterpret_cast<const uint32_t*>(&constants), sizeof(constants) / 4 });
    commands.traceRays(reflectionPipeline, width, height, 1,
                       { .sampledTextures = { textures.data(), textures.size() },
                         .storageTextures = { &reflection },
                         .buffers = { buffers.data(), buffers.size() } });
    commands.endGpuTimestamp();
}
