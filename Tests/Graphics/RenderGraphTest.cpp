#include <gtest/gtest.h>

#include "Graphics/IRenderGraph.h"

namespace
{
uint32_t countEvents(const RenderGraph& graph, DebugEventType type)
{
    uint32_t count = 0;
    const DebugEvent* events = graph.getLastDebugEvents();
    for (uint32_t i = 0; i < graph.getLastDebugEventCount(); ++i)
    {
        if (events[i].mType == type)
            ++count;
    }
    return count;
}
} // namespace

TEST(RenderGraphTest, MissingPassEntryPointsProduceExecutablePlan)
{
    RenderGraph graph;
    graph.beginFrame(nullptr, 64, 64, 0);

    graph.addComputePass("Compute");
    graph.addCopyPass("Copy");
    graph.addRayTracingPass("RayTracing");

    EXPECT_EQ(graph.buildExecutionPlan(), 3u);
    EXPECT_EQ(countEvents(graph, DebugEventType::Pass), 3u);
}

TEST(RenderGraphTest, UavReadAfterWriteInSameStateRecordsSynchronizationBarrier)
{
    RenderGraph graph;
    Buffer*     buffer = reinterpret_cast<Buffer*>(uintptr_t(0x1));

    graph.beginFrame(nullptr, 64, 64, 0);
    RGBuffer handle = graph.importBuffer("GeneratedBuffer", buffer, RESOURCE_STATE_COMMON, RESOURCE_STATE_COMMON);
    ASSERT_TRUE(handle.isValid());

    graph.addComputePass("WriteUav").write(handle, RESOURCE_STATE_UNORDERED_ACCESS);
    graph.addComputePass("ReadUav").read(handle, RESOURCE_STATE_UNORDERED_ACCESS);

    EXPECT_EQ(graph.buildExecutionPlan(), 5u);
    EXPECT_EQ(countEvents(graph, DebugEventType::Barrier), 2u);
    EXPECT_EQ(countEvents(graph, DebugEventType::FinalBarrier), 1u);

    const DebugEvent* events = graph.getLastDebugEvents();
    ASSERT_GE(graph.getLastDebugEventCount(), 3u);
    EXPECT_EQ(events[2].mType, DebugEventType::Barrier);
    EXPECT_EQ(events[2].mStateBefore, RESOURCE_STATE_UNORDERED_ACCESS);
    EXPECT_EQ(events[2].mStateAfter, RESOURCE_STATE_UNORDERED_ACCESS);
    EXPECT_TRUE(graph.wasImportedResourceWritten(handle));
}

TEST(RenderGraphTest, InternalResourcesTrackUseRangeWithoutFinalBarrier)
{
    RenderGraph graph;
    graph.beginFrame(nullptr, 64, 64, 0);

    BufferDesc bufferDesc = {
        .mSize = 256,
        .mStartState = RESOURCE_STATE_COMMON,
        .mDescriptors = DESCRIPTOR_TYPE_RW_BUFFER,
    };
    RGBuffer handle = graph.createBuffer("InternalBuffer", &bufferDesc);
    ASSERT_TRUE(handle.isValid());

    graph.addComputePass("WriteInternal").write(handle, RESOURCE_STATE_UNORDERED_ACCESS);
    graph.addComputePass("ReadInternal").read(handle, RESOURCE_STATE_GENERIC_READ);

    graph.execute(nullptr);

    EXPECT_TRUE(graph.isResourceUsed(handle));
    EXPECT_FALSE(graph.isResourceAllocated(handle));
    EXPECT_EQ(graph.getResourceFirstUse(handle), 0u);
    EXPECT_EQ(graph.getResourceLastUse(handle), 1u);
    EXPECT_EQ(countEvents(graph, DebugEventType::Pass), 2u);
    EXPECT_EQ(countEvents(graph, DebugEventType::FinalBarrier), 0u);
}

TEST(RenderGraphTest, UnusedInternalResourcesStayUnallocated)
{
    RenderGraph graph;
    graph.beginFrame(nullptr, 64, 64, 0);

    TextureDesc textureDesc = {
        .mWidth = 64,
        .mHeight = 64,
        .mDepth = 1,
        .mArraySize = 1,
        .mMipLevels = 1,
        .mSampleCount = SAMPLE_COUNT_1,
        .mFormat = TinyImageFormat_R8G8B8A8_UNORM,
        .mStartState = RESOURCE_STATE_COMMON,
        .mDescriptors = DESCRIPTOR_TYPE_TEXTURE,
    };
    RGTexture unused = graph.createTexture("UnusedTexture", &textureDesc);
    ASSERT_TRUE(unused.isValid());

    graph.addComputePass("NoResources");
    graph.execute(nullptr);

    EXPECT_FALSE(graph.isResourceUsed(unused));
    EXPECT_FALSE(graph.isResourceAllocated(unused));
    EXPECT_EQ(graph.getResourceFirstUse(unused), InvalidHandle);
    EXPECT_EQ(graph.getResourceLastUse(unused), InvalidHandle);
}

TEST(RenderGraphTest, ImportedTextureOnlyReceivesFinalTransition)
{
    RenderGraph graph;
    Texture*    texture = reinterpret_cast<Texture*>(uintptr_t(0x2));

    graph.beginFrame(nullptr, 64, 64, 0);
    RGTexture handle = graph.importTexture("ExternalTexture", texture, RESOURCE_STATE_COMMON, RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    ASSERT_TRUE(handle.isValid());

    graph.addComputePass("WriteTexture").write(handle, RESOURCE_STATE_UNORDERED_ACCESS);

    EXPECT_EQ(graph.buildExecutionPlan(), 3u);
    EXPECT_EQ(countEvents(graph, DebugEventType::Barrier), 1u);
    EXPECT_EQ(countEvents(graph, DebugEventType::FinalBarrier), 1u);
    EXPECT_TRUE(graph.wasImportedResourceWritten(handle));
}
