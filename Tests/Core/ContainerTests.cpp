#include <gtest/gtest.h>

#include "Core/IContainer.h"

static_assert(sizeof(hz::Array<int>) == sizeof(int*));
static_assert(sizeof(hz::FixedArray<int, 3>) == sizeof(int) * 3);
constexpr hz::FixedArray<int, 3> fixedValues{ .values = { 2, 4, 6 } };
static_assert(fixedValues.Size() == 3 && fixedValues[1] == 4);

class CoreContainerTest: public ::testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(InitMemoryAlloc(nullptr));
        initialStats = GetMemoryTrackingStats();
    }
    void TearDown() override
    {
        const MemoryTrackingStats stats = GetMemoryTrackingStats();
        if (stats.TrackingEnabled)
        {
            EXPECT_EQ(stats.LiveAllocationCount, initialStats.LiveAllocationCount);
            EXPECT_EQ(stats.LiveRequestedBytes, initialStats.LiveRequestedBytes);
        }
        ExitMemoryAlloc();
    }

    MemoryTrackingStats initialStats = {};
};

TEST_F(CoreContainerTest, ArrayPreservesElementsAndAliasedValuesAcrossGrowth)
{
    hz::Array<uint32_t> values;
    EXPECT_TRUE(values.Empty());
    EXPECT_EQ(values.begin(), values.end());
    values.Reserve(0);
    EXPECT_EQ(values.GetData(), nullptr);

    values.Reserve(4);
    const uint32_t Capacity = values.Capacity();
    for (uint32_t i = 0; i < Capacity; ++i)
        values.PushBack(i + 10);
    values.PushBack(values[0]);
    ASSERT_EQ(values.Size(), Capacity + 1);
    EXPECT_GT(values.Capacity(), Capacity);
    for (uint32_t i = 0; i < Capacity; ++i)
        EXPECT_EQ(values[i], i + 10);
    EXPECT_EQ(values[Capacity], 10u);

    values.Resize(Capacity + 4);
    EXPECT_EQ(values[Capacity + 1], 0u);
    EXPECT_EQ(values[Capacity + 3], 0u);
    values.PopBack();
    EXPECT_EQ(values.Size(), Capacity + 3);
    values.Resize(1);
    values.Resize(3);
    EXPECT_EQ(values[0], 10u);
    EXPECT_EQ(values[1], 0u);
    EXPECT_EQ(values[2], 0u);
}

TEST_F(CoreContainerTest, ArrayMovesStorageAndRetainsCapacityWhenCleared)
{
    const int      source[] = { 3, 5, 7 };
    hz::Array<int> values{ hz::Span<int>(source) };
    int* const     Data = values.GetData();
    const uint32_t Capacity = values.Capacity();
    hz::Array<int> moved(std::move(values));
    EXPECT_EQ(moved.GetData(), Data);
    EXPECT_EQ(values.GetData(), nullptr);
    EXPECT_TRUE(values.Empty());

    hz::Array<int> destination(12);
    destination = std::move(moved);
    EXPECT_TRUE(moved.Empty());
    const hz::Array<int>& view = destination;
    int                   sum = 0;
    for (const int value : view)
        sum += value;
    EXPECT_EQ(sum, 15);
    EXPECT_EQ(view[2], 7);
    hz::Array<int> copied(destination);
    copied[0] = 99;
    EXPECT_EQ(destination[0], 3);
    values = destination;
    EXPECT_NE(values.GetData(), destination.GetData());
    EXPECT_EQ(values[2], 7);
    values.Reset();
    destination.Clear();
    EXPECT_EQ(destination.GetData(), Data);
    EXPECT_EQ(destination.Capacity(), Capacity);
    EXPECT_TRUE(destination.Empty());
    destination.PushBack(9);
    EXPECT_EQ(destination[0], 9);
    destination.Reset();
    EXPECT_EQ(destination.Capacity(), 0u);
    EXPECT_EQ(destination.GetData(), nullptr);
    values.PushBack(11);
    EXPECT_EQ(values[0], 11);
}

struct alignas(64) FixedArrayObject
{
    static inline int liveCount = 0;
    FixedArrayObject(): pSelf(this) { ++liveCount; }
    ~FixedArrayObject()
    {
        EXPECT_EQ(pSelf, this);
        --liveCount;
    }
    FixedArrayObject(const FixedArrayObject&) = delete;
    FixedArrayObject& operator=(const FixedArrayObject&) = delete;

    const FixedArrayObject* pSelf;
    int                     value = 42;
};

TEST_F(CoreContainerTest, FixedArrayEmbedsObjectsWithoutAllocating)
{
    EXPECT_EQ(FixedArrayObject::liveCount, 0);
    const MemoryTrackingStats before = GetMemoryTrackingStats();
    {
        hz::FixedArray<FixedArrayObject, 3> objects;
        EXPECT_EQ(FixedArrayObject::liveCount, 3);
        EXPECT_EQ((uintptr_t)objects.GetData() % alignof(FixedArrayObject), 0u);
        EXPECT_EQ((void*)objects.GetData(), (void*)&objects);
        EXPECT_EQ(objects[2].value, 42);
        const MemoryTrackingStats after = GetMemoryTrackingStats();
        EXPECT_EQ(after.TotalAllocationCount, before.TotalAllocationCount);
    }
    EXPECT_EQ(FixedArrayObject::liveCount, 0);
}

struct NonDefaultConstructible
{
    NonDefaultConstructible() = delete;
};

TEST_F(CoreContainerTest, FixedArraySupportsZeroLengthAndValueCopies)
{
    hz::FixedArray<NonDefaultConstructible, 0> Empty;
    EXPECT_TRUE(Empty.Empty());
    EXPECT_EQ(Empty.GetData(), nullptr);
    EXPECT_EQ(Empty.begin(), Empty.end());
    hz::FixedArray<int, 3> zeroed{};
    for (const int value : zeroed)
        EXPECT_EQ(value, 0);

    hz::FixedArray<int, 3>       source{ .values = { 2, 4, 6 } };
    const hz::FixedArray<int, 3> copied = source;
    source[0] = 99;
    EXPECT_EQ(copied[0], 2);
    EXPECT_EQ(copied.end() - copied.begin(), 3);
}

struct alignas(64) MovableObject
{
    static inline int liveCount = 0;
    explicit MovableObject(int value = 0): value(value) { ++liveCount; }
    MovableObject(MovableObject&& other) noexcept: value(other.value)
    {
        other.value = -1;
        ++liveCount;
    }
    ~MovableObject()
    {
        EXPECT_EQ(pSelf, this);
        --liveCount;
    }
    MovableObject(const MovableObject&) = delete;
    MovableObject& operator=(const MovableObject&) = delete;

    const MovableObject* pSelf = this;
    int                  value;
};

TEST_F(CoreContainerTest, ArrayRelocatesAndDestroysAlignedMoveOnlyObjects)
{
    EXPECT_EQ(MovableObject::liveCount, 0);
    hz::Array<MovableObject> objects;
    objects.Reserve(4);
    const uint32_t Capacity = objects.Capacity();
    for (uint32_t i = 0; i < Capacity; ++i)
        objects.EmplaceBack((int)i + 10);
    objects.EmplaceBack(objects[0].value);
    EXPECT_EQ(MovableObject::liveCount, (int)objects.Size());
    EXPECT_EQ(objects[Capacity].value, 10);
    for (uint32_t i = 0; i < Capacity; ++i)
        EXPECT_EQ(objects[i].value, (int)i + 10);
    EXPECT_EQ((uintptr_t)objects.GetData() % alignof(MovableObject), 0u);
    objects.Resize(2);
    EXPECT_EQ(MovableObject::liveCount, 2);
    objects.PopBack();
    EXPECT_EQ(MovableObject::liveCount, 1);
    MovableObject* const     Data = objects.GetData();
    hz::Array<MovableObject> moved(std::move(objects));
    EXPECT_EQ(moved.GetData(), Data);
    EXPECT_TRUE(objects.Empty());
    moved.Clear();
    EXPECT_EQ(MovableObject::liveCount, 0);
}
