#include "AssetTestFixtures.h"
#include "../../Examples/Renderer/Source/RendererConstants.h"
#include "../../Examples/Renderer/Source/ClusteredLighting.h"
#include "../../Examples/Renderer/Source/DeferredLighting.h"
#include "../../Examples/Renderer/Shaders/ClusterGrid.h"
#include <cmath>
#include <d3d12sdklayers.h>

TEST_F(SceneAssetLoaderTest, LocalLightSourceRadiusBroadensSpecularAndPreservesDiffuse)
{
    const std::filesystem::path tests = std::filesystem::path(__FILE__).parent_path();
    SetResourceDirectoryPath(&fileSystem, MountProject, DirectoryOtherFiles, tests.string().c_str());
    hz::RenderContext context({ .pAppName = "LightSourceRadiusTest" });
    hz::GPUPipeline   pipeline =
        context.CreateComputePipeline({ .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "CompareSourceRadii" } },
                                                        .sourceDirectory = DirectoryOtherFiles,
                                                        .pFileName = "ClusteredLightingTests.slang" } });
    ASSERT_TRUE(pipeline.IsValid());
    hz::GPUBuffer output = context.CreateBuffer({ .Size = sizeof(Vector4),
                                                  .ElementCount = 1,
                                                  .StructStride = sizeof(Vector4),
                                                  .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
                                                  .StartState = RESOURCE_STATE_UNORDERED_ACCESS,
                                                  .Descriptors = DESCRIPTOR_TYPE_RW_BUFFER });
    hz::GPUBuffer readback = context.CreateBuffer({ .Size = sizeof(Vector4),
                                                    .usage = RESOURCE_MEMORY_USAGE_GPU_TO_CPU,
                                                    .StartState = RESOURCE_STATE_COPY_DEST,
                                                    .Flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT });
    ASSERT_TRUE(output.IsValid() && readback.IsValid());
    hz::CommandList& commands = context.AcquireCommandList();
    commands.SetPipeline(pipeline);
    commands.SetRootConstant({ 0, output.GetUavIndex() });
    commands.Dispatch(1, 1, 1, { .storageBuffers = { &output } });
    commands.CopyBuffer(readback, 0, output, 0, sizeof(Vector4));
    context.Wait(context.Submit(commands));
    const float* values = (const float*)readback.Get()->pCpuMappedAddress;
    ASSERT_NE(values, nullptr);
    EXPECT_GT(values[0], 0.0f);
    EXPECT_FLOAT_EQ(values[0], values[1]);
    EXPECT_GT(values[3], values[2]);
}

TEST_F(SceneAssetLoaderTest, ClusteredLightingKeepsLightsOverlappingFarPlane)
{
    const std::filesystem::path shaders = std::filesystem::path(__FILE__).parent_path() / "../../Examples/Renderer/Shaders";
    SetResourceDirectoryPath(&fileSystem, MountProject, DirectoryShaderSources, shaders.string().c_str());
    hz::RenderContext context({ .pAppName = "ClusteredLightingTest" });
    ClusteredLighting clusters(context);
    LightingFrameData frame;
    hz::FrameUpload uploadArena(context, 3);
    uploadArena.BeginFrame(1);
    hz::RenderLight   lights[2] = {};
    const float       depths[] = { 18.0f, 16.5f };
    uint32_t          active[] = { 0, 1 };
    for (uint32_t i = 0; i < TF_ARRAY_COUNT(lights); ++i)
    {
        lights[i].Type = hz::LightType::Point;
        lights[i].ready = true;
        lights[i].InfluenceRadius = 1.5f;
        lights[i].transform = Matrix4::translation(Vector3(0, 0, -depths[i]));
    }
    const FrameData camera = { .ViewProjection = Matrix4::perspectiveRH(PI / 2, 1.0f, 0.1f, 16.0f),
                               .Near = 0.1f,
                               .Far = 16,
                               .Width = 32,
                               .Height = 32 };
    hz::GPUBuffer masks = context.CreateBuffer(ClusteredLighting::GetTileMaskDesc().Resolve(camera.Width, camera.Height));
    ASSERT_TRUE(clusters.Prepare(frame, uploadArena, masks, lights, active, camera));
    ASSERT_EQ(clusters.GetLightIndices().Count, 1u);
    EXPECT_EQ(clusters.GetLightIndices().Data[0], 1u);
}

TEST_F(SceneAssetLoaderTest, ClusteredLightingFallsBackOnlyAboveVisibleLightLimit)
{
    const std::filesystem::path shaders = std::filesystem::path(__FILE__).parent_path() / "../../Examples/Renderer/Shaders";
    SetResourceDirectoryPath(&fileSystem, MountProject, DirectoryShaderSources, shaders.string().c_str());
    hz::RenderContext            context({ .pAppName = "ClusteredLightingTest" });
    ClusteredLighting            clusters(context);
    LightingFrameData            frame;
    hz::FrameUpload uploadArena(context, 3);
    uploadArena.BeginFrame(1);
    constexpr uint32_t           count = ClusteredLighting::MaxClusteredLights + 1;
    std::vector<hz::RenderLight> lights(count);
    std::vector<uint32_t>        active(count);
    for (uint32_t i = 0; i < count; ++i)
    {
        lights[i].Type = hz::LightType::Point;
        lights[i].ready = true;
        lights[i].InfluenceRadius = 0.01f;
        lights[i].transform = Matrix4::translation(Vector3(0, 0, -3));
        active[i] = i;
    }
    const FrameData                 camera = { .ViewProjection = Matrix4::perspectiveRH(PI / 2, 1.0f, 0.1f, 16.0f),
                                               .Near = 0.1f,
                                               .Far = 16,
                                               .Width = LIGHT_TILE_SIZE,
                                               .Height = LIGHT_TILE_SIZE };
    const hz::Span<hz::RenderLight> lightSpan(lights.data(), count);
    hz::GPUBuffer masks = context.CreateBuffer(ClusteredLighting::GetTileMaskDesc().Resolve(camera.Width, camera.Height));
    ASSERT_TRUE(clusters.Prepare(frame, uploadArena, masks, lightSpan, { active.data(), 1 }, camera));
    ASSERT_EQ(frame.ClusterLightingTileLightBitMasksIndex, masks.GetSrvIndex());
    constexpr uint32_t maxBitMasks = (ClusteredLighting::MaxClusteredLights + LIGHTS_PER_BITMASK - 1) / LIGHTS_PER_BITMASK;
    EXPECT_GE(masks.GetSize(), maxBitMasks * sizeof(uint32_t));
    EXPECT_EQ(clusters.GetLightIndices().Count, 1u);
    EXPECT_FALSE(clusters.Prepare(frame, uploadArena, masks, lightSpan, { active.data(), count }, camera));
    lights.back().transform = Matrix4::translation(Vector3(0, 0, -30));
    ASSERT_TRUE(clusters.Prepare(frame, uploadArena, masks, lightSpan, { active.data(), count }, camera));
    EXPECT_EQ(clusters.GetLightIndices().Count, count - 1);
    lights.back().InfluenceRadius = 10;
    EXPECT_FLOAT_EQ(lights.back().InfluenceRadius, 10.0f);
    ASSERT_TRUE(clusters.Prepare(frame, uploadArena, masks, lightSpan, { active.data(), count }, camera));
    EXPECT_EQ(clusters.GetLightIndices().Count, count - 1);
    lights.back().InfluenceRadius = 20;
    EXPECT_FLOAT_EQ(lights.back().InfluenceRadius, 20.0f);
    EXPECT_FALSE(clusters.Prepare(frame, uploadArena, masks, lightSpan, { active.data(), count }, camera));
    EXPECT_EQ(frame.ClusterLightingZBinsIndex, UINT32_MAX);
    EXPECT_EQ(frame.ClusterLightingTileLightBitMasksIndex, UINT32_MAX);
    EXPECT_EQ(clusters.GetLightIndices().Count, 0u);
    ASSERT_TRUE(clusters.Prepare(frame, uploadArena, masks, lightSpan, {}, camera));
    EXPECT_EQ(frame.ClusterLightingZBinsIndex, uploadArena.GetBuffer().GetSrvIndex());
    EXPECT_EQ(frame.ClusterLightingTileLightBitMasksIndex, UINT32_MAX);
    EXPECT_EQ(clusters.GetLightIndices().Count, 0u);
}

TEST_F(SceneAssetLoaderTest, ClusteredLightingMatchesFullLoopAcrossTilesAndDepths)
{
    const std::filesystem::path tests = std::filesystem::path(__FILE__).parent_path();
    SetResourceDirectoryPath(&fileSystem, MountProject, DirectoryShaderSources,
                             (tests / "../../Examples/Renderer/Shaders").string().c_str());
    SetResourceDirectoryPath(&fileSystem, MountProject, DirectoryOtherFiles, tests.string().c_str());
    hz::RenderContext context({ .pAppName = "ClusteredLightingTest", .enableGpuValidation = true });
    ClusteredLighting clusters(context);
    hz::FrameUpload uploadArena(context, 3);
    hz::GPUPipeline   compare =
        context.CreateComputePipeline({ .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "CompareLighting" } },
                                                        .sourceDirectory = DirectoryOtherFiles,
                                                        .pFileName = "ClusteredLightingTests.slang" } });
    ASSERT_TRUE(compare.IsValid());

    const hz::LightType special[] = { hz::LightType::Directional, hz::LightType::Directional, hz::LightType::Point,
                                      hz::LightType::Sky,         hz::LightType::Sky,         hz::LightType::Point,
                                      hz::LightType::Point,       hz::LightType::Point,       hz::LightType::Point };
    constexpr uint32_t  PrefixCount = TF_ARRAY_COUNT(special), Total = PrefixCount + 33, PixelCapacity = 65 * 35 * 5;
    hz::RenderLight     lights[Total];
    hz::GpuLightData    packed[Total];
    uint32_t            active[Total];
    const Vector3       outside[] = { Vector3(0, 0, 4), Vector3(0, 0, -20), Vector3(-10, 0, -3) };
    for (uint32_t i = 0; i < Total; ++i)
    {
        active[i] = i;
        lights[i].ready = i != 5;
        lights[i].Type = i < PrefixCount ? special[i] : hz::LightType(1 + i % 3);
        lights[i].InfluenceRadius = i < 6 ? 10.0f : 1.5f;
        Vector3 pos((int(i % 5) - 2) * 0.9f, (int(i % 3) - 1) * 0.4f, -0.5f - float(i % 7));
        if (i >= 6 && i < PrefixCount)
        {
            pos = outside[i - 6];
        }
        if (i == PrefixCount)
        {
            pos = Vector3(0, 0, 0.2f);
        }
        if (i == PrefixCount + 1)
        {
            pos = Vector3(0, 0, -15.5f);
        }
        if (i == PrefixCount + 2)
        {
            pos = Vector3(3.4f, 0, -3.0f);
        }
        lights[i].transform = Matrix4::translation(pos);
        packed[i] = { .PositionAndInvRadius = Vector4(pos, lights[i].InfluenceRadius > 0 ? 1.0f / lights[i].InfluenceRadius : 0.0f),
                      .DirectionAndType = Vector4(0.0f, 0.0f, -1.0f, float(lights[i].Type)),
                      .Color = Vector4(1.0f, 0.7f, 0.4f, 1.0f),
                      .HalfAxisX = Vector4(0.1f, 0.0f, 0.0f, 1.0f),
                      .HalfAxisY = Vector4(0.0f, 0.12f, 0.0f, 0.1f),
                      .SpotAngles = float2(0.5f, 4.0f) };
    }
    const auto upload = [&](const void* data, uint32_t bytes, uint32_t stride)
    {
        return context.CreateBuffer({ .Size = bytes,
                                      .ElementCount = bytes / (stride ? stride : 4),
                                      .StructStride = stride,
                                      .pInitialData = data,
                                      .initialDataSize = bytes,
                                      .usage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU,
                                      .StartState = RESOURCE_STATE_SHADER_RESOURCE,
                                      .Descriptors = stride ? DESCRIPTOR_TYPE_BUFFER : DESCRIPTOR_TYPE_BUFFER_RAW });
    };
    const hz::GPUBuffer lightBuffer = upload(packed, sizeof(packed), 0);
    hz::GPUBuffer       output = context.CreateBuffer({ .Size = PixelCapacity * 16,
                                                        .ElementCount = PixelCapacity,
                                                        .StructStride = 16,
                                                        .usage = RESOURCE_MEMORY_USAGE_GPU_ONLY,
                                                        .StartState = RESOURCE_STATE_UNORDERED_ACCESS,
                                                        .Descriptors = DESCRIPTOR_TYPE_RW_BUFFER });
    hz::GPUBuffer       readback = context.CreateBuffer({ .Size = output.GetSize(),
                                                          .usage = RESOURCE_MEMORY_USAGE_GPU_TO_CPU,
                                                          .StartState = RESOURCE_STATE_COPY_DEST,
                                                          .Flags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT });
    const uint32_t      counts[] = { 0, 1, 31, 32, 33, 0, 1 };
    for (uint32_t step = 0; step < TF_ARRAY_COUNT(counts); ++step)
    {
        // Only the deepest light reaches the far sample. Exclude globals so a dropped second mask leaves it black.
        const bool        secondBatch = counts[step] == 33;
        const uint32_t    first = secondBatch ? PrefixCount : 0;
        const uint32_t    count = PrefixCount + counts[step] - first, width = step % 2 ? 63 : 65, height = 35;
        uploadArena.BeginFrame(step % 3);
        LightingFrameData frame;
        Matrix4           vp = Matrix4::perspectiveRH(PI / 2, float(height) / width, 0.1f, 16);
        vp.setRow(0, vp.getRow(0) + vp.getRow(3) * (0.5f / width));
        const FrameData camera = { .ViewProjection = vp, .Near = 0.1f, .Far = 16, .Width = width, .Height = height };
        hz::GPUBuffer masks = context.CreateBuffer(ClusteredLighting::GetTileMaskDesc().Resolve(camera.Width, camera.Height));
        ASSERT_TRUE(clusters.Prepare(frame, uploadArena, masks, lights, { active + first, count }, camera));
        const hz::Span<uint32_t> sorted = clusters.GetLightIndices();
        ASSERT_EQ(sorted.Count, counts[step] + (secondBatch ? 0u : 1u));
        ASSERT_EQ(frame.ClusterLightingTileLightBitMasksIndex, masks.GetSrvIndex());
        std::vector<uint32_t> indices;
        for (uint32_t i = first; i < first + count; ++i)
        {
            if (hz::IsLocalLight(lights[i].Type) && lights[i].ready)
            {
                indices.push_back(i);
            }
        }
        const uint32_t localCount = uint32_t(indices.size());
        indices.insert(indices.end(), sorted.begin(), sorted.end());
        const hz::GPUBuffer activeList = upload(indices.data(), uint32_t(indices.size() * sizeof(uint32_t)), 0);
        frame.LocalLightsIndex = lightBuffer.GetSrvIndex();
        frame.LocalLightCount = localCount;
        frame.ActiveLocalLightsIndex = activeList.GetSrvIndex();
        const hz::GPUBuffer lighting = upload(&frame, sizeof(frame), sizeof(frame));
        hz::CommandList&    commands = context.AcquireCommandList();
        clusters.BuildLightTileMasks(commands, camera, masks, lightBuffer, activeList, localCount);
        const hz::GPUBuffer* buffers[] = { &uploadArena.GetBuffer(), &lighting, &lightBuffer, &activeList, &masks };
        commands.SetPipeline(compare);
        commands.SetRootConstant({ lighting.GetSrvIndex(), output.GetUavIndex(), width, height });
        const Vector4 projection(float(vp.getRow(0).getZ()), float(vp.getRow(1).getZ()), float(vp.getRow(0).getX()),
                                 float(vp.getRow(1).getY()));
        commands.SetRootConstants(projection, 4);
        commands.SetRootConstants(0.1f, 8);
        commands.Dispatch((width + 7) / 8, (height + 7) / 8, 5, { .buffers = buffers, .storageBuffers = { &output } });
        commands.CopyBuffer(readback, 0, output, 0, width * height * 5 * 16);
        const hz::SubmitHandle submission = context.Submit(commands);
        uploadArena.EndFrame(submission);
        context.Wait(submission);
        const float* values = (const float*)readback.Get()->pCpuMappedAddress;
        if (secondBatch)
        {
            const uint32_t farCenter = (4 * height + height / 2) * width + width / 2;
            ASSERT_GT(values[farCenter * 4 + 3], 1e-4f);
        }
        float peak = 0;
        for (uint32_t p = 0; p < width * height * 5; ++p)
        {
            for (uint32_t c = 0; c < 3; ++c)
            {
                ASSERT_LE(values[4 * p + c], 1.0f) << "step " << step << ", sample " << p;
            }
            peak = std::fmax(peak, values[4 * p + 3]);
        }
        EXPECT_GT(peak, 0.0f);
    }
    ID3D12Device* device = nullptr;
    ASSERT_EQ(output.Get()->dx.pResource->GetDevice(IID_PPV_ARGS(&device)), S_OK);
    EXPECT_EQ(device->GetDeviceRemovedReason(), S_OK);
    ID3D12InfoQueue* info = nullptr;
    if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&info))))
    {
        D3D12_MESSAGE_SEVERITY  errors[] = { D3D12_MESSAGE_SEVERITY_ERROR, D3D12_MESSAGE_SEVERITY_CORRUPTION };
        D3D12_INFO_QUEUE_FILTER filter = {};
        filter.AllowList.NumSeverities = TF_ARRAY_COUNT(errors);
        filter.AllowList.pSeverityList = errors;
        if (SUCCEEDED(info->PushRetrievalFilter(&filter)))
        {
            EXPECT_EQ(info->GetNumStoredMessagesAllowedByRetrievalFilter(), 0u);
            info->PopRetrievalFilter();
        }
        info->Release();
    }
    device->Release();
}
