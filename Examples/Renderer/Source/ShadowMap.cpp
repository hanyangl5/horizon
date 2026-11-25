// Adapted from Cookbook Renderer ShadowPass; see ../Cookbook-LICENSE.txt.
#include "ShadowMap.h"
#include "RendererConstants.h"
#include "Profiler/IProfiler.h"
#include "Core/ILog.h"
#include <float.h>
#include <math.h>

// Rounds a light-space extent up to a 1/8 octave step: the texel Size stays fixed while the scene bounds
// move a little, and grows by at most 12.5% at a step instead of doubling like a power-of-two ladder.
static float QuantizeShadowExtent(float Extent)
{
    const float Base = exp2f(floorf(log2f(Extent)));
    return Base * ceilf(Extent / Base * 8.0f) * 0.125f;
}

ShadowMap::ShadowMap(hz::RenderContext& Context): Context(Context)
{
    Pipeline = Context.CreateGraphicsPipeline({ .shaderDesc = { .stages = { { .stage = SHADER_STAGE_VERT, .pEntryPoint = "VSIndirect" },
                                                                            { .stage = SHADER_STAGE_FRAG, .pEntryPoint = "PSShadow" } },
                                                                .pFileName = "ShadowMap.slang" },
                                                .rasterizer = { .cullMode = CULL_MODE_NONE,
                                                                .depthBias = 1,
                                                                .slopeScaledDepthBias = 2.0f,
                                                                .fillMode = FILL_MODE_SOLID,
                                                                .frontFace = FRONT_FACE_CCW },
                                                .Depth = { .depthTest = true, .depthWrite = true, .depthFunc = CMP_LEQUAL },
                                                .depthStencilFormat = DepthFormat,
                                                .indirectDrawId = true,
                                                .Name = "Renderer.ShadowMap" });
    Sampler = Context.CreateSampler(
        { .AddressU = ADDRESS_MODE_CLAMP_TO_EDGE, .AddressV = ADDRESS_MODE_CLAMP_TO_EDGE, .CompareFunc = CMP_LEQUAL });
    SurfaceSampler = Context.CreateSampler();
    for (uint32_t Index = 0; Index < ShadowFrames.Size(); ++Index)
    {
        ShadowFrames[Index] = Context.CreateBuffer({ .Size = sizeof(FrameData),
                                                     .ElementCount = 1,
                                                     .StructStride = sizeof(FrameData),
                                                     .Name = "Renderer.ShadowCamera",
                                                     .usage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU,
                                                     .StartState = RESOURCE_STATE_SHADER_RESOURCE,
                                                     .Descriptors = DESCRIPTOR_TYPE_BUFFER,
                                                     .Flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT });
    }
    ASSERT(Pipeline.IsValid() && Sampler.IsValid());
}

// Slots 0..7 of the RootConstant0 block shared with Geometry.slang.
struct ShadowBindingConstants
{
    uint32_t InstanceIndex = 0;
    uint32_t MaterialIndex = 0;
    uint32_t MaterialId = 0;
    uint32_t FrameIndex;
    uint32_t InstancesIndex;
    uint32_t MaterialsIndex;
    uint32_t TexturesIndex;
    uint32_t SamplerIndex;
};
static_assert(sizeof(ShadowBindingConstants) == 8 * sizeof(uint32_t));

Matrix4 ShadowMap::Prepare(const hz::SceneView& View, const Vector3& LightDirection)
{
    PROFILER_SET_CPU_SCOPE_AUTO_NAMED("ShadowMap::Prepare");
    const Vector3 Z = -LightDirection;
    const Vector3 Up = fabsf((float)Z[2]) < 0.99f ? Vector3(0, 0, 1) : Vector3(0, 1, 0);
    const Vector3 X = normalize(cross(Up, Z));
    const Vector3 Y = cross(Z, X);
    const Matrix4 LightView = orthoInverse(Matrix4(Vector4(X, 0), Vector4(Y, 0), Vector4(Z, 0), Vector4(0, 0, 0, 1)));
    float         Minimum[3] = { FLT_MAX, FLT_MAX, FLT_MAX }, Maximum[3] = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
    bool          HasBounds = false;
    for (uint32_t MeshBatchIndex : View.Scene.GetActiveInstances())
    {
        const hz::RenderInstance& Instance = View.Scene.GetInstances().Data[MeshBatchIndex];
        if (!View.Scene.GetPrimitives().Data[Instance.primitive.index].instanceCount)
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
        // bounds move by a few centimetres, and quantize the extents so the texel Size itself is stable.
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
    const Matrix4   ViewProjection = Projection * LightView;
    const FrameData Camera = { .ViewProjection = ViewProjection,
                               .PreviousViewProjection = ViewProjection,
                               .InverseViewProjection = inverse(ViewProjection),
                               .Eye = Vector4(0, 0, 0, 1) };

    const uint32_t Backing = View.FrameIndex;
    const bool     CameraUpdated = Context.UpdateBuffer(ShadowFrames[Backing], 0, &Camera, sizeof(Camera));
    ASSERT(CameraUpdated);
    return ViewProjection;
}

void ShadowMap::Execute(hz::CommandList& Commands, const hz::SceneView& View, const hz::GpuDrawList& Draws, const hz::GPUTexture& Depth)
{
    PROFILER_SET_CPU_SCOPE_AUTO_NAMED("ShadowMap::Execute");
    const uint32_t Backing = View.FrameIndex;
    Commands.BeginGpuTimestamp("Shadow Map");
    Commands.BeginRendering({ .depthAttachment = { .pTexture = &Depth,
                                                   .loadAction = LOAD_ACTION_CLEAR,
                                                   .storeAction = STORE_ACTION_STORE,
                                                   .ClearValue = { .Depth = 1 } } },
                            Draws.GetDependencies(ShadowFrames[Backing], View, &Draws.GetCommands(Backing, Draws.GetShadowCasterList())));
    Commands.SetPipeline(Pipeline);
    Commands.SetViewport(0, 0, (float)Resolution, (float)Resolution);
    Commands.SetScissor(0, 0, Resolution, Resolution);
    const hz::GpuSceneBuffers&   Gpu = View.GpuScene.GetBuffers();
    const ShadowBindingConstants Binding = {
        .FrameIndex = ShadowFrames[Backing].GetSrvIndex(),
        .InstancesIndex = Gpu.instances.IsValid() ? Gpu.instances.GetSrvIndex() : UINT32_MAX,
        .MaterialsIndex = Gpu.materials.IsValid() ? Gpu.materials.GetSrvIndex() : UINT32_MAX,
        .TexturesIndex = Gpu.textures.IsValid() ? Gpu.textures.GetSrvIndex() : UINT32_MAX,
        .SamplerIndex = SurfaceSampler.GetIndex(),
    };
    Commands.SetRootConstants(Binding);
    Commands.SetRootConstant({ Draws.GetDrawData(Backing).GetSrvIndex() }, GeometryDrawSlot);
    if (Draws.GetCount(Backing))
    {
        Commands.DrawIndirect(Draws.GetCommands(Backing, Draws.GetShadowCasterList()), 0, Draws.GetCount(Backing));
    }
    Commands.EndRendering();
    Commands.EndGpuTimestamp();
}
