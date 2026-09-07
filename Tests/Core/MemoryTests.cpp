#include <gtest/gtest.h>

#include <stdint.h>

#define IMEMORY_FROM_HEADER
#include "Core/IMemory.h"
#include "Core/IThread.h"

namespace
{
struct LifetimeProbe
{
    static int ctorCount;
    static int dtorCount;

    explicit LifetimeProbe(int inValue): value(inValue) { ++ctorCount; }
    ~LifetimeProbe() { ++dtorCount; }

    int value;
};

int LifetimeProbe::ctorCount = 0;
int LifetimeProbe::dtorCount = 0;
} // namespace

// Verifies aligned, zero-initialized, and resized allocations through the core memory helpers.
TEST(CoreMemoryTest, AllocationHelpersAllocateAlignedZeroedAndResizableMemory)
{
    ASSERT_TRUE(initMemAlloc(nullptr));
    const MemoryTrackingStats initialStats = memGetTrackingStats();
    if (initialStats.mTrackingEnabled)
    {
        EXPECT_EQ(initialStats.mLiveRequestedBytes, 0u);
        EXPECT_EQ(initialStats.mLiveActualBytes, 0u);
        EXPECT_EQ(initialStats.mLiveAllocationCount, 0u);
    }

    void* aligned = tf_memalign(64, 128);
    ASSERT_NE(aligned, nullptr);
    EXPECT_EQ((uintptr_t)aligned % 64u, 0u);
    const MemoryTrackingStats alignedStats = memGetTrackingStats();
    if (alignedStats.mTrackingEnabled)
    {
        EXPECT_EQ(alignedStats.mLiveRequestedBytes, 128u);
        EXPECT_GE(alignedStats.mLiveActualBytes, alignedStats.mLiveRequestedBytes);
        EXPECT_EQ(alignedStats.mLiveSlackBytes, alignedStats.mLiveActualBytes - alignedStats.mLiveRequestedBytes);
        EXPECT_EQ(alignedStats.mLiveAllocationCount, 1u);
    }
    tf_free(aligned);

    auto* zeroed = (unsigned char*)tf_calloc(8, sizeof(unsigned char));
    ASSERT_NE(zeroed, nullptr);
    for (size_t i = 0; i < 8; ++i)
    {
        EXPECT_EQ(zeroed[i], 0u);
    }
    tf_free(zeroed);

    auto* bytes = (unsigned char*)tf_malloc(4);
    ASSERT_NE(bytes, nullptr);
    bytes[0] = 1;
    bytes[1] = 2;
    bytes[2] = 3;
    bytes[3] = 4;

    bytes = (unsigned char*)tf_realloc(bytes, 8);
    ASSERT_NE(bytes, nullptr);
    EXPECT_EQ(bytes[0], 1u);
    EXPECT_EQ(bytes[1], 2u);
    EXPECT_EQ(bytes[2], 3u);
    EXPECT_EQ(bytes[3], 4u);
    tf_free(bytes);

    void* resized = tf_realloc(nullptr, 32);
    ASSERT_NE(resized, nullptr);
    EXPECT_EQ(tf_realloc(resized, 0), nullptr);
    EXPECT_EQ(tf_calloc(SIZE_MAX, 2), nullptr);

    const MemoryTrackingStats finalStats = memGetTrackingStats();
    if (finalStats.mTrackingEnabled)
    {
        EXPECT_EQ(finalStats.mLiveRequestedBytes, 0u);
        EXPECT_EQ(finalStats.mLiveActualBytes, 0u);
        EXPECT_EQ(finalStats.mLiveSlackBytes, 0u);
        EXPECT_EQ(finalStats.mLiveAllocationCount, 0u);
        EXPECT_GE(finalStats.mPeakRequestedBytes, 128u);
        EXPECT_GE(finalStats.mPeakActualBytes, finalStats.mPeakRequestedBytes);
        EXPECT_GE(finalStats.mTotalAllocationCount, 3u);
        EXPECT_GE(finalStats.mReallocationCount, 1u);
        EXPECT_EQ(finalStats.mFailedAllocationCount, 1u);
    }

    exitMemAlloc();
}

TEST(CoreMemoryTest, ConcurrentReallocationsLeaveNoLiveAllocations)
{
    ASSERT_TRUE(initMemAlloc(nullptr));
    ThreadHandle threads[4] = {};
    uint32_t     started = 0;
    ThreadDesc   desc = {
          .pFunc =
            [](void*)
        {
            for (uint32_t i = 0; i < 256; ++i)
            {
                unsigned char* bytes = (unsigned char*)tf_malloc(32);
                ASSERT_NE(bytes, nullptr);
                bytes[0] = 42;
                unsigned char* resized = (unsigned char*)tf_realloc(bytes, 4096 + i * 16);
                if (!resized)
                {
                    tf_free(bytes);
                    FAIL() << "reallocation failed";
                }
                EXPECT_EQ(resized[0], 42);
                tf_free(resized);
            }
        },
    };
    for (; started < TF_ARRAY_COUNT(threads); ++started)
    {
        if (!initThread(&desc, &threads[started]))
            break;
    }
    for (uint32_t i = 0; i < started; ++i)
        joinThread(threads[i]);
    EXPECT_EQ(started, TF_ARRAY_COUNT(threads));
    const MemoryTrackingStats stats = memGetTrackingStats();
    if (stats.mTrackingEnabled)
    {
        EXPECT_EQ(stats.mLiveAllocationCount, 0u);
        EXPECT_EQ(stats.mLiveRequestedBytes, 0u);
        EXPECT_EQ(stats.mLiveActualBytes, 0u);
        EXPECT_EQ(stats.mReallocationCount, started * 256u);
    }
    exitMemAlloc();
}

// Verifies that tf_new and tf_delete invoke constructors and destructors exactly once.
TEST(CoreMemoryTest, TfNewAndTfDeleteRespectObjectLifetime)
{
    LifetimeProbe::ctorCount = 0;
    LifetimeProbe::dtorCount = 0;

    LifetimeProbe* probe = tf_new(LifetimeProbe, 123);
    ASSERT_NE(probe, nullptr);
    EXPECT_EQ(probe->value, 123);
    EXPECT_EQ(LifetimeProbe::ctorCount, 1);
    EXPECT_EQ(LifetimeProbe::dtorCount, 0);

    tf_delete(probe);

    EXPECT_EQ(LifetimeProbe::ctorCount, 1);
    EXPECT_EQ(LifetimeProbe::dtorCount, 1);
}
