// Adapted from Cookbook Renderer ShadowPass; see ../Cookbook-LICENSE.txt.
#include "ShadowMap.h"
#include "RendererConstants.h"
#include "Profiler/IProfiler.h"
#include "Core/ILog.h"
#include <float.h>
#include <math.h>

// Rounds a light-space extent up to a 1/8 octave step: the texel size stays fixed while the scene bounds
// move a little, and grows by at most 12.5% at a step instead of doubling like a power-of-two ladder.
static float QuantizeShadowExtent(float Extent)
{
    const float Base = exp2f(floorf(log2f(Extent)));
    return Base * ceilf(Extent / Base * 8.0f) * 0.125f;
}

ShadowMap::ShadowMap(hz::RenderContext& Context, uint32_t Resolution): Context(Context), Resolution(Resolution)
{
    Pipeline = Context.createGraphicsPipeline({ .shaderDesc = { .stages = { { .stage = SHADER_STAGE_VERT, .pEntryPoint = "VSIndirect" },
                                                                           { .stage = SHADER_STAGE_FRAG, .pEntryPoint = "PSShadow" } },
                                                               .pFileName = "ShadowMap.slang" },
                                                .rasterizer = { .cullMode = CULL_MODE_NONE,
                                                                .depthBias = 1,
                                                                .slopeScaledDepthBias = 2.0f,
                                                                .fillMode = FILL_MODE_SOLID,
                                                                .frontFace = FRONT_FACE_CCW },
                                                .depth = { .depthTest = true, .depthWrite = true, .depthFunc = CMP_LEQUAL },
                                                .depthStencilFormat = hz::Format::D16_UNORM,
                                                .indirectDrawId = true,
                                                .pName = "Renderer.ShadowMap" });
    Depth = Context.createTexture({ .width = Resolution,
                                    .height = Resolution,
                                    .format = hz::Format::D16_UNORM,
                                    .startState = RESOURCE_STATE_DEPTH_WRITE,
                                    .descriptors = DESCRIPTOR_TYPE_TEXTURE,
                                    .renderTarget = true,
                                    .pName = "Renderer.ShadowMap" });
    Sampler = Context.createSampler(
        { .addressU = ADDRESS_MODE_CLAMP_TO_EDGE, .addressV = ADDRESS_MODE_CLAMP_TO_EDGE, .compareFunc = CMP_LEQUAL });
    SurfaceSampler = Context.createSampler();
    for (uint32_t Index = 0; Index < ShadowFrames.size(); ++Index)
    {
        ShadowFrames[Index] = Context.createBuffer({ .size = 208,
                                                     .elementCount = 1,
                                                     .structStride = 208,
                                                     .pName = "Renderer.ShadowCamera",
                                                     .usage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU,
                                                     .startState = RESOURCE_STATE_SHADER_RESOURCE,
                                                     .descriptors = DESCRIPTOR_TYPE_BUFFER,
                                                     .flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT });
        LightingFrames[Index] = Context.createBuffer({ .size = sizeof(LightingFrameData),
                                                       .elementCount = 1,
                                                       .structStride = sizeof(LightingFrameData),
                                                       .pName = "Renderer.LightingFrame",
                                                       .usage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU,
                                                       .startState = RESOURCE_STATE_SHADER_RESOURCE,
                                                       .descriptors = DESCRIPTOR_TYPE_BUFFER,
                                                       .flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT });
    }
    ASSERT(Pipeline.isValid() && Depth.isValid() && Sampler.isValid());
}

// Slots 0..7 of the RootConstant0 block shared with Geometry.slang.
struct ShadowBindingConstants
{
    uint32_t instanceIndex = 0;
    uint32_t materialIndex = 0;
    uint32_t materialId = 0;
    uint32_t frameIndex;
    uint32_t instancesIndex;
    uint32_t materialsIndex;
    uint32_t texturesIndex;
    uint32_t samplerIndex;
};
static_assert(sizeof(ShadowBindingConstants) == 8 * sizeof(uint32_t));

void ShadowMap::Execute(hz::CommandList& Commands, const SceneView& View, const GpuDrawList& Draws)
{
    PROFILER_SET_CPU_SCOPE_AUTO_NAMED("ShadowMap::Execute");
    Vector3 Z(0, 1, 0);
    Data.lightDirection = Vector4(0, -1, 0, 0);
    Data.shadowEnabled = 0;
    for (uint32_t MeshBatchIndex : View.Scene.getActiveLights())
    {
        const hz::RenderLight& Light = View.Scene.getLights().pData[MeshBatchIndex];
        if (!Light.ready || Light.type != hz::LightType::Directional)
        {
            continue;
        }
        Z = normalize(Light.transform.getCol2().getXYZ());
        Data.lightDirection = Vector4(-Z, Light.intensity * exp2f(Light.exposure) * LuminousEfficacy);
        Data.lightColor = Vector4(Light.color[0], Light.color[1], Light.color[2], 1);
        Data.shadowEnabled = 1;
        break;
    }
    const Vector3 Up = fabsf((float)Z[2]) < 0.99f ? Vector3(0, 0, 1) : Vector3(0, 1, 0);
    const Vector3 X = normalize(cross(Up, Z));
    const Vector3 Y = cross(Z, X);
    const Matrix4 LightView = orthoInverse(Matrix4(Vector4(X, 0), Vector4(Y, 0), Vector4(Z, 0), Vector4(0, 0, 0, 1)));
    float         Minimum[3] = { FLT_MAX, FLT_MAX, FLT_MAX }, Maximum[3] = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
    bool          HasBounds = false;
    for (uint32_t MeshBatchIndex : View.Scene.getActiveInstances())
    {
        const hz::RenderInstance& Instance = View.Scene.getInstances().pData[MeshBatchIndex];
        if (!View.Scene.getPrimitives().pData[Instance.primitive.index].instanceCount)
        {
            continue;
        }
        for (uint32_t Corner = 0; Corner < 8; ++Corner)
        {
            const Vector4 Position = LightView * Vector4((Corner & 1) ? Instance.bounds.maximum[0] : Instance.bounds.minimum[0],
                                                         (Corner & 2) ? Instance.bounds.maximum[1] : Instance.bounds.minimum[1],
                                                         (Corner & 4) ? Instance.bounds.maximum[2] : Instance.bounds.minimum[2], 1);
            for (uint32_t Axis = 0; Axis < 3; ++Axis)
            {
                Minimum[Axis] = fminf(Minimum[Axis], (float)Position[Axis]);
                Maximum[Axis] = fmaxf(Maximum[Axis], (float)Position[Axis]);
            }
        }
        HasBounds = true;
    }
    Matrix4 Projection = Matrix4::identity();
    if (HasBounds)
    {
        // Snap the light-space volume to a whole texel grid so the map does not crawl when the scene
        // bounds move by a few centimetres, and quantize the extents so the texel size itself is stable.
        // The margin keeps enough slack that snapping the centre cannot push geometry out of the volume.
        const float RawX = fmaxf(Maximum[0] - Minimum[0], 0.001f);
        const float RawY = fmaxf(Maximum[1] - Minimum[1], 0.001f);
        const float Margin = 4.0f / (float)Resolution;
        const float ExtentX = QuantizeShadowExtent(RawX * (1.0f + Margin));
        const float ExtentY = QuantizeShadowExtent(RawY * (1.0f + Margin));
        const float DepthRange = fmaxf(Maximum[2] - Minimum[2], 0.001f);
        const float TexelX = ExtentX / (float)Resolution;
        const float TexelY = ExtentY / (float)Resolution;
        const float CenterX = floorf((Maximum[0] + Minimum[0]) * 0.5f / TexelX) * TexelX;
        const float CenterY = floorf((Maximum[1] + Minimum[1]) * 0.5f / TexelY) * TexelY;
        Projection = Matrix4(Vector4(2 / ExtentX, 0, 0, 0), Vector4(0, 2 / ExtentY, 0, 0), Vector4(0, 0, -1 / DepthRange, 0),
                             Vector4(-2 * CenterX / ExtentX, -2 * CenterY / ExtentY, Maximum[2] / DepthRange, 1));
    }
    Data.shadowViewProjection = Projection * LightView;
    struct ShadowFrame
    {
        Matrix4 current;
        Matrix4 previous;
        Matrix4 inverse;
        Vector4 eye;
    };
    const ShadowFrame Camera = { Data.shadowViewProjection, Data.shadowViewProjection, inverse(Data.shadowViewProjection),
                                 Vector4(0, 0, 0, 1) };

    const uint32_t Backing = View.FrameIndex;
    const bool     CameraUpdated = Context.updateBuffer(ShadowFrames[Backing], 0, &Camera, sizeof(Camera));
    const bool     LightUpdated = Context.updateBuffer(LightingFrames[Backing], 0, &Data, sizeof(Data));
    ASSERT(CameraUpdated && LightUpdated);
    ++Data.frameIndex;
    Commands.beginGpuTimestamp("Shadow Map");
    Commands.beginRendering({ .depthAttachment = { .pTexture = &Depth,
                                                   .loadAction = LOAD_ACTION_CLEAR,
                                                   .storeAction = STORE_ACTION_STORE,
                                                   .clearValue = { .depth = 1 } } },
                            Draws.GetDependencies(ShadowFrames[Backing], View, &Draws.GetShadowCommands(Backing)));
    Commands.setPipeline(Pipeline);
    Commands.setViewport(0, 0, (float)Resolution, (float)Resolution);
    Commands.setScissor(0, 0, Resolution, Resolution);
    const hz::GpuSceneBuffers& Gpu = View.GpuScene.getBuffers();
    const ShadowBindingConstants Binding = {
        .frameIndex = ShadowFrames[Backing].getSrvIndex(),
        .instancesIndex = Gpu.instances.isValid() ? Gpu.instances.getSrvIndex() : UINT32_MAX,
        .materialsIndex = Gpu.materials.isValid() ? Gpu.materials.getSrvIndex() : UINT32_MAX,
        .texturesIndex = Gpu.textures.isValid() ? Gpu.textures.getSrvIndex() : UINT32_MAX,
        .samplerIndex = SurfaceSampler.getIndex(),
    };
    Commands.setRootConstants(Binding);
    Commands.setRootConstant({ Draws.GetDrawData(Backing).getSrvIndex() }, GeometryDrawSlot);
    if (Draws.GetCount(Backing))
    {
        Commands.drawIndirect(Draws.GetShadowCommands(Backing), 0, Draws.GetCount(Backing));
    }
    Commands.endRendering();
    Commands.endGpuTimestamp();
}
