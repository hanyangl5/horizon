// Adapted from Cookbook Renderer ShadowPass; see ../Cookbook-LICENSE.txt.
#include "ShadowMap.h"
#include "RendererConstants.h"
#include "Profiler/IProfiler.h"
#include "Core/ILog.h"
#include <float.h>
#include <math.h>

// Rounds a light-space extent up to a 1/8 octave step: the texel size stays fixed while the scene bounds
// move a little, and grows by at most 12.5% at a step instead of doubling like a power-of-two ladder.
static float quantizeShadowExtent(float extent)
{
    const float base = exp2f(floorf(log2f(extent)));
    return base * ceilf(extent / base * 8.0f) * 0.125f;
}

ShadowMap::ShadowMap(hz::RenderContext& context, uint32_t newResolution): context(context), resolution(newResolution)
{
    pipeline = context.createGraphicsPipeline({ .shaderDesc = { .stages = { { .stage = SHADER_STAGE_VERT, .pEntryPoint = "VSIndirect" },
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
    depth = context.createTexture({ .width = resolution,
                                    .height = resolution,
                                    .format = hz::Format::D16_UNORM,
                                    .startState = RESOURCE_STATE_DEPTH_WRITE,
                                    .descriptors = DESCRIPTOR_TYPE_TEXTURE,
                                    .renderTarget = true,
                                    .pName = "Renderer.ShadowMap" });
    sampler = context.createSampler(
        { .addressU = ADDRESS_MODE_CLAMP_TO_EDGE, .addressV = ADDRESS_MODE_CLAMP_TO_EDGE, .compareFunc = CMP_LEQUAL });
    surfaceSampler = context.createSampler();
    for (uint32_t i = 0; i < shadowFrames.size(); ++i)
    {
        shadowFrames[i] = context.createBuffer({ .size = 208,
                                                 .elementCount = 1,
                                                 .structStride = 208,
                                                 .pName = "Renderer.ShadowCamera",
                                                 .usage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU,
                                                 .startState = RESOURCE_STATE_SHADER_RESOURCE,
                                                 .descriptors = DESCRIPTOR_TYPE_BUFFER,
                                                 .flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT });
        lightingFrames[i] = context.createBuffer({ .size = sizeof(LightingFrameData),
                                                   .elementCount = 1,
                                                   .structStride = sizeof(LightingFrameData),
                                                   .pName = "Renderer.LightingFrame",
                                                   .usage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU,
                                                   .startState = RESOURCE_STATE_SHADER_RESOURCE,
                                                   .descriptors = DESCRIPTOR_TYPE_BUFFER,
                                                   .flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT });
    }
    ASSERT(pipeline.isValid() && depth.isValid() && sampler.isValid());
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
void ShadowMap::execute(hz::CommandList& commands, const SceneView& view, const GpuDrawList& draws)
{
    PROFILER_SET_CPU_SCOPE_AUTO_NAMED("ShadowMap::execute");
    Vector3 z(0, 1, 0);
    data.lightDirection = Vector4(0, -1, 0, 0);
    data.shadowEnabled = 0;
    for (uint32_t meshBatchIndex : view.scene.getActiveLights())
    {
        const hz::RenderLight& light = view.scene.getLights().pData[meshBatchIndex];
        if (!light.ready || light.type != hz::LightType::Directional)
        {
            continue;
        }
        z = normalize(light.transform.getCol2().getXYZ());
        data.lightDirection = Vector4(-z, light.intensity * exp2f(light.exposure) * luminousEfficacy);
        data.lightColor = Vector4(light.color[0], light.color[1], light.color[2], 1);
        data.shadowEnabled = 1;
        break;
    }
    const Vector3 up = fabsf((float)z[2]) < 0.99f ? Vector3(0, 0, 1) : Vector3(0, 1, 0);
    const Vector3 x = normalize(cross(up, z));
    const Vector3 y = cross(z, x);
    const Matrix4 lightView = orthoInverse(Matrix4(Vector4(x, 0), Vector4(y, 0), Vector4(z, 0), Vector4(0, 0, 0, 1)));
    float         minimum[3] = { FLT_MAX, FLT_MAX, FLT_MAX }, maximum[3] = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
    bool          hasBounds = false;
    for (uint32_t meshBatchIndex : view.scene.getActiveInstances())
    {
        const hz::RenderInstance& instance = view.scene.getInstances().pData[meshBatchIndex];
        if (!view.scene.getPrimitives().pData[instance.primitive.index].instanceCount)
        {
            continue;
        }
        for (uint32_t c = 0; c < 8; ++c)
        {
            const Vector4 corner = lightView * Vector4((c & 1) ? instance.bounds.maximum[0] : instance.bounds.minimum[0],
                                                       (c & 2) ? instance.bounds.maximum[1] : instance.bounds.minimum[1],
                                                       (c & 4) ? instance.bounds.maximum[2] : instance.bounds.minimum[2], 1);
            for (uint32_t axis = 0; axis < 3; ++axis)
            {
                minimum[axis] = fminf(minimum[axis], (float)corner[axis]);
                maximum[axis] = fmaxf(maximum[axis], (float)corner[axis]);
            }
        }
        hasBounds = true;
    }
    Matrix4 projection = Matrix4::identity();
    if (hasBounds)
    {
        // Snap the light-space volume to a whole texel grid so the map does not crawl when the scene
        // bounds move by a few centimetres, and quantize the extents so the texel size itself is stable.
        // The margin keeps enough slack that snapping the centre cannot push geometry out of the volume.
        const float rawX = fmaxf(maximum[0] - minimum[0], 0.001f);
        const float rawY = fmaxf(maximum[1] - minimum[1], 0.001f);
        const float margin = 4.0f / (float)resolution;
        const float extentX = quantizeShadowExtent(rawX * (1.0f + margin));
        const float extentY = quantizeShadowExtent(rawY * (1.0f + margin));
        const float dz = fmaxf(maximum[2] - minimum[2], 0.001f);
        const float texelX = extentX / (float)resolution;
        const float texelY = extentY / (float)resolution;
        const float centerX = floorf((maximum[0] + minimum[0]) * 0.5f / texelX) * texelX;
        const float centerY = floorf((maximum[1] + minimum[1]) * 0.5f / texelY) * texelY;
        projection = Matrix4(Vector4(2 / extentX, 0, 0, 0), Vector4(0, 2 / extentY, 0, 0), Vector4(0, 0, -1 / dz, 0),
                             Vector4(-2 * centerX / extentX, -2 * centerY / extentY, maximum[2] / dz, 1));
    }
    data.shadowViewProjection = projection * lightView;
    struct ShadowFrame
    {
        Matrix4 current;
        Matrix4 previous;
        Matrix4 inverse;
        Vector4 eye;
    };
    const ShadowFrame camera = { data.shadowViewProjection, data.shadowViewProjection, inverse(data.shadowViewProjection),
                                 Vector4(0, 0, 0, 1) };

    const uint32_t backing = view.frameIndex;
    const bool     cameraUpdated = context.updateBuffer(shadowFrames[backing], 0, &camera, sizeof(camera));
    const bool     lightUpdated = context.updateBuffer(lightingFrames[backing], 0, &data, sizeof(data));
    ASSERT(cameraUpdated && lightUpdated);
    ++data.frameIndex;
    commands.beginGpuTimestamp("Shadow Map");
    commands.beginRendering({ .depthAttachment = { .pTexture = &depth,
                                                   .loadAction = LOAD_ACTION_CLEAR,
                                                   .storeAction = STORE_ACTION_STORE,
                                                   .clearValue = { .depth = 1 } } },
                            draws.getDependencies(shadowFrames[backing], view, &draws.getShadowCommands(backing)));
    commands.setPipeline(pipeline);
    commands.setViewport(0, 0, (float)resolution, (float)resolution);
    commands.setScissor(0, 0, resolution, resolution);
    const hz::GpuSceneBuffers& gpu = view.gpuScene.getBuffers();
    const ShadowBindingConstants binding = {
        .frameIndex = shadowFrames[backing].getSrvIndex(),
        .instancesIndex = gpu.instances.isValid() ? gpu.instances.getSrvIndex() : UINT32_MAX,
        .materialsIndex = gpu.materials.isValid() ? gpu.materials.getSrvIndex() : UINT32_MAX,
        .texturesIndex = gpu.textures.isValid() ? gpu.textures.getSrvIndex() : UINT32_MAX,
        .samplerIndex = surfaceSampler.getIndex(),
    };
    commands.setRootConstants(binding);
    commands.setRootConstant({ draws.getDrawData(backing).getSrvIndex() }, kGeometryDrawSlot);
    if (draws.getCount(backing))
    {
        commands.drawIndirect(draws.getShadowCommands(backing), 0, draws.getCount(backing));
    }
    commands.endRendering();
    commands.endGpuTimestamp();
}
