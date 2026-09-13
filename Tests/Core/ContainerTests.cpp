#include <gtest/gtest.h>

#include "Core/IContainer.h"

static_assert(sizeof(hz::Array<int>) == sizeof(int*));
static_assert(sizeof(hz::FixedArray<int, 3>) == sizeof(int) * 3);
constexpr hz::FixedArray<int, 3> fixedValues{ .values = { 2, 4, 6 } };
static_assert(fixedValues.size() == 3 && fixedValues[1] == 4);

class CoreContainerTest: public ::testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(initMemAlloc(nullptr));
        initialStats = memGetTrackingStats();
    }
    void TearDown() override
    {
        const MemoryTrackingStats stats = memGetTrackingStats();
        if (stats.trackingEnabled)
        {
            EXPECT_EQ(stats.liveAllocationCount, initialStats.liveAllocationCount);
            EXPECT_EQ(stats.liveRequestedBytes, initialStats.liveRequestedBytes);
        }
        exitMemAlloc();
    }

    MemoryTrackingStats initialStats = {};
};

TEST_F(CoreContainerTest, ArrayPreservesElementsAndAliasedValuesAcrossGrowth)
{
    hz::Array<uint32_t> values;
    EXPECT_TRUE(values.empty());
    EXPECT_EQ(values.begin(), values.end());
    values.reserve(0);
    EXPECT_EQ(values.data(), nullptr);

    values.reserve(4);
    const uint32_t capacity = values.capacity();
    for (uint32_t i = 0; i < capacity; ++i)
        values.pushBack(i + 10);
    values.pushBack(values[0]);
    ASSERT_EQ(values.size(), capacity + 1);
    EXPECT_GT(values.capacity(), capacity);
    for (uint32_t i = 0; i < capacity; ++i)
        EXPECT_EQ(values[i], i + 10);
    EXPECT_EQ(values[capacity], 10u);

    values.resize(capacity + 4);
    EXPECT_EQ(values[capacity + 1], 0u);
    EXPECT_EQ(values[capacity + 3], 0u);
    values.popBack();
    EXPECT_EQ(values.size(), capacity + 3);
    values.resize(1);
    values.resize(3);
    EXPECT_EQ(values[0], 10u);
    EXPECT_EQ(values[1], 0u);
    EXPECT_EQ(values[2], 0u);
}

TEST_F(CoreContainerTest, ArrayMovesStorageAndRetainsCapacityWhenCleared)
{
    const int      source[] = { 3, 5, 7 };
    hz::Array<int> values{ hz::Span<int>(source) };
    int* const     pData = values.data();
    const uint32_t capacity = values.capacity();
    hz::Array<int> moved(std::move(values));
    EXPECT_EQ(moved.data(), pData);
    EXPECT_EQ(values.data(), nullptr);
    EXPECT_TRUE(values.empty());

    hz::Array<int> destination(12);
    destination = std::move(moved);
    EXPECT_TRUE(moved.empty());
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
    EXPECT_NE(values.data(), destination.data());
    EXPECT_EQ(values[2], 7);
    values.reset();
    destination.clear();
    EXPECT_EQ(destination.data(), pData);
    EXPECT_EQ(destination.capacity(), capacity);
    EXPECT_TRUE(destination.empty());
    destination.pushBack(9);
    EXPECT_EQ(destination[0], 9);
    destination.reset();
    EXPECT_EQ(destination.capacity(), 0u);
    EXPECT_EQ(destination.data(), nullptr);
    values.pushBack(11);
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
    const MemoryTrackingStats before = memGetTrackingStats();
    {
        hz::FixedArray<FixedArrayObject, 3> objects;
        EXPECT_EQ(FixedArrayObject::liveCount, 3);
        EXPECT_EQ((uintptr_t)objects.data() % alignof(FixedArrayObject), 0u);
        EXPECT_EQ((void*)objects.data(), (void*)&objects);
        EXPECT_EQ(objects[2].value, 42);
        const MemoryTrackingStats after = memGetTrackingStats();
        EXPECT_EQ(after.totalAllocationCount, before.totalAllocationCount);
    }
    EXPECT_EQ(FixedArrayObject::liveCount, 0);
}

struct NonDefaultConstructible
{
    NonDefaultConstructible() = delete;
};

TEST_F(CoreContainerTest, FixedArraySupportsZeroLengthAndValueCopies)
{
    hz::FixedArray<NonDefaultConstructible, 0> empty;
    EXPECT_TRUE(empty.empty());
    EXPECT_EQ(empty.data(), nullptr);
    EXPECT_EQ(empty.begin(), empty.end());
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
    objects.reserve(4);
    const uint32_t capacity = objects.capacity();
    for (uint32_t i = 0; i < capacity; ++i)
        objects.emplaceBack((int)i + 10);
    objects.emplaceBack(objects[0].value);
    EXPECT_EQ(MovableObject::liveCount, (int)objects.size());
    EXPECT_EQ(objects[capacity].value, 10);
    for (uint32_t i = 0; i < capacity; ++i)
        EXPECT_EQ(objects[i].value, (int)i + 10);
    EXPECT_EQ((uintptr_t)objects.data() % alignof(MovableObject), 0u);
    objects.resize(2);
    EXPECT_EQ(MovableObject::liveCount, 2);
    objects.popBack();
    EXPECT_EQ(MovableObject::liveCount, 1);
    MovableObject* const     pData = objects.data();
    hz::Array<MovableObject> moved(std::move(objects));
    EXPECT_EQ(moved.data(), pData);
    EXPECT_TRUE(objects.empty());
    moved.clear();
    EXPECT_EQ(MovableObject::liveCount, 0);
}
