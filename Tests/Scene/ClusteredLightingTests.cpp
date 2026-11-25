#include "AssetTestFixtures.h"
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
    hz::GPUPipeline pipeline = context.CreateComputePipeline(
        { .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "CompareSourceRadii" } },
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
    commands.SetRootConstant({ 0, 0, output.GetUavIndex() });
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
    const Matrix4 vp = Matrix4::perspectiveRH(PI / 2, 1.0f, 0.1f, 16.0f);
    ASSERT_TRUE(clusters.Prepare(0, lights, active, UINT32_MAX, vp, 0.1f, 16.0f, 32, 32));
    const uint8_t*     data = static_cast<const uint8_t*>(clusters.GetData(0).Get()->pCpuMappedAddress);
    const ClusterGrid& grid = *reinterpret_cast<const ClusterGrid*>(data);
    ASSERT_EQ(grid.GlobalCount, 0u);
    ASSERT_EQ(grid.FiniteCount, 1u);
    EXPECT_EQ(*reinterpret_cast<const uint32_t*>(data + grid.IndexOffset), 1u);
}

TEST_F(SceneAssetLoaderTest, ClusteredLightingMatchesFullLoopAcrossTilesAndDepths)
{
    const std::filesystem::path tests = std::filesystem::path(__FILE__).parent_path();
    SetResourceDirectoryPath(&fileSystem, MountProject, DirectoryShaderSources,
                             (tests / "../../Examples/Renderer/Shaders").string().c_str());
    SetResourceDirectoryPath(&fileSystem, MountProject, DirectoryOtherFiles, tests.string().c_str());
    hz::RenderContext context({ .pAppName = "ClusteredLightingTest", .enableGpuValidation = true });
    ClusteredLighting clusters(context);
    hz::GPUPipeline   compare =
        context.CreateComputePipeline({ .shaderDesc = { .stages = { { .stage = SHADER_STAGE_COMP, .pEntryPoint = "CompareLighting" } },
                                                        .sourceDirectory = DirectoryOtherFiles,
                                                        .pFileName = "ClusteredLightingTests.slang" } });
    ASSERT_TRUE(compare.IsValid());

    const hz::LightType special[] = { hz::LightType::Directional, hz::LightType::Directional, hz::LightType::Point,
                                      hz::LightType::Sky,        hz::LightType::Sky,        hz::LightType::Point,
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
        lights[i].InfluenceRadius = i < 6 ? 0.0f : 1.5f;
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
                      .DirectionAndType = Vector4(0, 0, -1, float(lights[i].Type)),
                      .Color = Vector4(lights[i].ready ? 1.0f : 0.0f, lights[i].ready ? 0.7f : 0.0f, lights[i].ready ? 0.4f : 0.0f, 1),
                      .HalfAxisX = Vector4(0.1f, 0, 0, 1),
                      .HalfAxisY = Vector4(0, 0.12f, 0, 0.1f),
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
    const hz::GPUBuffer lightBuffer = upload(packed, sizeof(packed), 0), activeBuffer = upload(active, sizeof(active), 0);
    const hz::GPUBuffer finiteActiveBuffer = upload(active + PrefixCount, (Total - PrefixCount) * sizeof(uint32_t), 0);
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
        // Only the deepest light reaches the far sample. Exclude globals so a dropped second word leaves it black.
        const bool     secondWord = counts[step] == 33;
        const uint32_t first = secondWord ? PrefixCount : 0;
        const uint32_t count = PrefixCount + counts[step] - first, width = step % 2 ? 63 : 65, height = 35, slot = step % 3;
        Matrix4        vp = Matrix4::perspectiveRH(PI / 2, float(height) / width, 0.1f, 16);
        vp.setRow(0, vp.getRow(0) + vp.getRow(3) * (0.5f / width));
        ASSERT_TRUE(clusters.Prepare(slot, lights, { active + first, count }, 0, vp, 0.1f, 16, width, height));
        const hz::GPUBuffer& data = clusters.GetData(slot);
        const ClusterGrid&   grid = *(const ClusterGrid*)data.Get()->pCpuMappedAddress;
        ASSERT_EQ(grid.FiniteCount, counts[step]);
        ASSERT_EQ(grid.GlobalCount, secondWord ? 0u : 1u);
        const hz::GPUBuffer&    activeList = secondWord ? finiteActiveBuffer : activeBuffer;
        const LightingFrameData frame = { .LightsIndex = lightBuffer.GetSrvIndex(),
                                          .LightCount = count,
                                          .ActiveLightsIndex = activeList.GetSrvIndex(),
                                          .MainDirectionalIndex = 0 };
        const hz::GPUBuffer     lighting = upload(&frame, sizeof(frame), sizeof(frame));
        hz::CommandList&        commands = context.AcquireCommandList();
        const hz::GPUBuffer*    mask = clusters.GetMask(slot);
        if (mask)
        {
            clusters.BuildMasks(commands, slot);
        }
        const hz::GPUBuffer* buffers[] = { &data, &lighting, &lightBuffer, &activeList, mask };
        commands.SetPipeline(compare);
        commands.SetRootConstant({ data.GetSrvIndex(), lighting.GetSrvIndex(), output.GetUavIndex() });
        commands.Dispatch((width + 7) / 8, (height + 7) / 8, 5, { .buffers = { buffers, mask ? 5u : 4u }, .storageBuffers = { &output } });
        commands.CopyBuffer(readback, 0, output, 0, width * height * 5 * 16);
        context.Wait(context.Submit(commands));
        const float* values = (const float*)readback.Get()->pCpuMappedAddress;
        if (secondWord)
        {
            const uint32_t farCenter = (4 * height + height / 2) * width + width / 2;
            ASSERT_GT(values[farCenter * 4 + 3], 1e-4f);
        }
        float        peak = 0;
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
