#include <gtest/gtest.h>

#include <stdint.h>

#define IMEMORY_FROM_HEADER
#include "Core/IMemory.h"

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

    void* aligned = tf_memalign(64, 128);
    ASSERT_NE(aligned, nullptr);
    EXPECT_EQ((uintptr_t)aligned % 64u, 0u);
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
