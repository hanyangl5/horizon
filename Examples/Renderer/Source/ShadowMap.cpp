// Adapted from Cookbook Renderer ShadowPass; see ../Cookbook-LICENSE.txt.
#include "ShadowMap.h"
#include "Core/ILog.h"
#include <float.h>
#include <math.h>

ShadowMap::ShadowMap(hz::RenderContext& context): context(context)
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
    depth = context.createTexture({ .width = 512,
                                    .height = 512,
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

void ShadowMap::execute(hz::CommandList& commands, const hz::SceneFrameSnapshot& scene, const GpuDrawList& draws)
{
    Vector3 z(0, 1, 0);
    data.lightDirection = Vector4(0, -1, 0, 0);
    data.shadowEnabled = 0;
    for (uint32_t slot : scene.getActiveLights())
    {
        const hz::RenderLight& light = scene.getLights().pData[slot];
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
    const Matrix4 view = orthoInverse(Matrix4(Vector4(x, 0), Vector4(y, 0), Vector4(z, 0), Vector4(0, 0, 0, 1)));
    float         minimum[3] = { FLT_MAX, FLT_MAX, FLT_MAX }, maximum[3] = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
    bool          hasBounds = false;
    for (uint32_t slot : scene.getActiveInstances())
    {
        const hz::RenderInstance& instance = scene.getInstances().pData[slot];
        if (!scene.getObjects().pData[instance.object.index].instanceCount)
        {
            continue;
        }
        for (uint32_t c = 0; c < 8; ++c)
        {
            const Vector4 corner = view * Vector4((c & 1) ? instance.bounds.maximum[0] : instance.bounds.minimum[0],
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
        const float dx = fmaxf(maximum[0] - minimum[0], 0.001f);
        const float dy = fmaxf(maximum[1] - minimum[1], 0.001f);
        const float dz = fmaxf(maximum[2] - minimum[2], 0.001f);
        projection = Matrix4(Vector4(2 / dx, 0, 0, 0), Vector4(0, 2 / dy, 0, 0), Vector4(0, 0, -1 / dz, 0),
                             Vector4(-(maximum[0] + minimum[0]) / dx, -(maximum[1] + minimum[1]) / dy, maximum[2] / dz, 1));
    }
    data.shadowViewProjection = projection * view;
    struct ShadowFrame
    {
        Matrix4 current;
        Matrix4 previous;
        Matrix4 inverse;
        Vector4 eye;
    };
    const ShadowFrame camera = { data.shadowViewProjection, data.shadowViewProjection, inverse(data.shadowViewProjection),
                                 Vector4(0, 0, 0, 1) };

    const uint32_t backing = scene.getBackingIndex();
    const bool     cameraUpdated = context.updateBuffer(shadowFrames[backing], 0, &camera, sizeof(camera));
    const bool     lightUpdated = context.updateBuffer(lightingFrames[backing], 0, &data, sizeof(data));
    ASSERT(cameraUpdated && lightUpdated);
    ++data.frameIndex;
    commands.beginGpuTimestamp("Shadow Map");
    commands.beginRendering({ .depthAttachment = { .pTexture = &depth,
                                                   .loadAction = LOAD_ACTION_CLEAR,
                                                   .storeAction = STORE_ACTION_STORE,
                                                   .clearValue = { .depth = 1 } } },
                            draws.getDependencies(shadowFrames[backing], scene, draws.getShadowCommands(backing)));
    commands.setPipeline(pipeline);
    commands.setViewport(0, 0, 512, 512);
    commands.setScissor(0, 0, 512, 512);
    commands.setRootConstant({ 0, 0, 0, shadowFrames[backing].getSrvIndex(),
                               scene.getBuffers().instances.isValid() ? scene.getBuffers().instances.getSrvIndex() : UINT32_MAX,
                               scene.getBuffers().materials.isValid() ? scene.getBuffers().materials.getSrvIndex() : UINT32_MAX,
                               scene.getBuffers().textures.isValid() ? scene.getBuffers().textures.getSrvIndex() : UINT32_MAX,
                               surfaceSampler.getIndex() });
    commands.setRootConstant({ draws.getRecords(backing).getSrvIndex() }, 11);
    if (draws.getCount(backing))
    {
        commands.drawIndirect(draws.getShadowCommands(backing), 0, draws.getCount(backing));
    }
    commands.endRendering();
    commands.endGpuTimestamp();
}
