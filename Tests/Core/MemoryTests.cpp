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
    ASSERT_TRUE(InitMemoryAlloc(nullptr));
    const MemoryTrackingStats initialStats = GetMemoryTrackingStats();
    if (initialStats.TrackingEnabled)
    {
        EXPECT_EQ(initialStats.LiveRequestedBytes, 0u);
        EXPECT_EQ(initialStats.LiveActualBytes, 0u);
        EXPECT_EQ(initialStats.LiveAllocationCount, 0u);
    }

    void* aligned = MemAllocateAligned(64, 128);
    ASSERT_NE(aligned, nullptr);
    EXPECT_EQ((uintptr_t)aligned % 64u, 0u);
    const MemoryTrackingStats alignedStats = GetMemoryTrackingStats();
    if (alignedStats.TrackingEnabled)
    {
        EXPECT_EQ(alignedStats.LiveRequestedBytes, 128u);
        EXPECT_GE(alignedStats.LiveActualBytes, alignedStats.LiveRequestedBytes);
        EXPECT_EQ(alignedStats.LiveSlackBytes, alignedStats.LiveActualBytes - alignedStats.LiveRequestedBytes);
        EXPECT_EQ(alignedStats.LiveAllocationCount, 1u);
    }
    MemDeallocate(aligned);

    auto* zeroed = (unsigned char*)MemCallocate(8, sizeof(unsigned char));
    ASSERT_NE(zeroed, nullptr);
    for (size_t i = 0; i < 8; ++i)
    {
        EXPECT_EQ(zeroed[i], 0u);
    }
    MemDeallocate(zeroed);

    auto* bytes = (unsigned char*)MemAllocate(4);
    ASSERT_NE(bytes, nullptr);
    bytes[0] = 1;
    bytes[1] = 2;
    bytes[2] = 3;
    bytes[3] = 4;

    bytes = (unsigned char*)MemReallocate(bytes, 8);
    ASSERT_NE(bytes, nullptr);
    EXPECT_EQ(bytes[0], 1u);
    EXPECT_EQ(bytes[1], 2u);
    EXPECT_EQ(bytes[2], 3u);
    EXPECT_EQ(bytes[3], 4u);
    MemDeallocate(bytes);

    void* resized = MemReallocate(nullptr, 32);
    ASSERT_NE(resized, nullptr);
    EXPECT_EQ(MemReallocate(resized, 0), nullptr);
    EXPECT_EQ(MemCallocate(SIZE_MAX, 2), nullptr);

    const MemoryTrackingStats finalStats = GetMemoryTrackingStats();
    if (finalStats.TrackingEnabled)
    {
        EXPECT_EQ(finalStats.LiveRequestedBytes, 0u);
        EXPECT_EQ(finalStats.LiveActualBytes, 0u);
        EXPECT_EQ(finalStats.LiveSlackBytes, 0u);
        EXPECT_EQ(finalStats.LiveAllocationCount, 0u);
        EXPECT_GE(finalStats.PeakRequestedBytes, 128u);
        EXPECT_GE(finalStats.PeakActualBytes, finalStats.PeakRequestedBytes);
        EXPECT_GE(finalStats.TotalAllocationCount, 3u);
        EXPECT_GE(finalStats.ReallocationCount, 1u);
        EXPECT_EQ(finalStats.FailedAllocationCount, 1u);
    }

    ExitMemoryAlloc();
}

TEST(CoreMemoryTest, ConcurrentReallocationsLeaveNoLiveAllocations)
{
    ASSERT_TRUE(InitMemoryAlloc(nullptr));
    ThreadHandle threads[4] = {};
    uint32_t     started = 0;
    ThreadDesc   desc = {
          .pFunc =
            [](void*)
        {
            for (uint32_t i = 0; i < 256; ++i)
            {
                unsigned char* bytes = (unsigned char*)MemAllocate(32);
                ASSERT_NE(bytes, nullptr);
                bytes[0] = 42;
                unsigned char* resized = (unsigned char*)MemReallocate(bytes, 4096 + i * 16);
                if (!resized)
                {
                    MemDeallocate(bytes);
                    FAIL() << "reallocation failed";
                }
                EXPECT_EQ(resized[0], 42);
                MemDeallocate(resized);
            }
        },
    };
    for (; started < TF_ARRAY_COUNT(threads); ++started)
    {
        if (!InitThread(&desc, &threads[started]))
            break;
    }
    for (uint32_t i = 0; i < started; ++i)
        JoinThread(threads[i]);
    EXPECT_EQ(started, TF_ARRAY_COUNT(threads));
    const MemoryTrackingStats stats = GetMemoryTrackingStats();
    if (stats.TrackingEnabled)
    {
        EXPECT_EQ(stats.LiveAllocationCount, 0u);
        EXPECT_EQ(stats.LiveRequestedBytes, 0u);
        EXPECT_EQ(stats.LiveActualBytes, 0u);
        EXPECT_EQ(stats.ReallocationCount, started * 256u);
    }
    ExitMemoryAlloc();
}

// Verifies that MemNew and MemDelete invoke constructors and destructors exactly once.
TEST(CoreMemoryTest, TfNewAndTfDeleteRespectObjectLifetime)
{
    LifetimeProbe::ctorCount = 0;
    LifetimeProbe::dtorCount = 0;

    LifetimeProbe* probe = MemNew(LifetimeProbe, 123);
    ASSERT_NE(probe, nullptr);
    EXPECT_EQ(probe->value, 123);
    EXPECT_EQ(LifetimeProbe::ctorCount, 1);
    EXPECT_EQ(LifetimeProbe::dtorCount, 0);

    MemDelete(probe);

    EXPECT_EQ(LifetimeProbe::ctorCount, 1);
    EXPECT_EQ(LifetimeProbe::dtorCount, 1);
}
