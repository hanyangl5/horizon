#include <gtest/gtest.h>

#include "Core/IAlgorithm.h"

namespace
{
struct SortItem
{
    int key;
    int originalIndex;
};

bool lessByKey(const void* lhs, const void* rhs, void*)
{
    const SortItem& left = *(const SortItem*)lhs;
    const SortItem& right = *(const SortItem*)rhs;
    return left.key < right.key;
}

void expectIntArrayEquals(const int32_t* actual, const int32_t* expected, size_t count)
{
    for (size_t i = 0; i < count; ++i)
    {
        EXPECT_EQ(actual[i], expected[i]) << "at index " << i;
    }
}
} // namespace

// Verifies that sortInt32 orders a mixed input containing negatives and duplicates.
TEST(CoreAlgorithmsTest, SortInt32OrdersLargeInput)
{
    int32_t values[] = {
        17, 3, 42, -5, 9, 0, 12, 8, 8, 1, 99, -12, 4, 15, 27, 6, 18, 21, 2, 7,
        13, 11, 5, 19, 16, 10, 14, 20, 25, 24, 23, 22, 26, 28, 30, 29, 31, 32, -1, 17,
    };
    const int32_t expected[] = {
        -12, -5, -1, 0, 1, 2, 3, 4, 5, 6,
        7, 8, 8, 9, 10, 11, 12, 13, 14, 15,
        16, 17, 17, 18, 19, 20, 21, 22, 23, 24,
        25, 26, 27, 28, 29, 30, 31, 32, 42, 99,
    };

    sortInt32(values, TF_ARRAY_COUNT(values));

    expectIntArrayEquals(values, expected, TF_ARRAY_COUNT(values));
}

// Verifies that stableSort keeps the original relative order of elements with equal keys.
TEST(CoreAlgorithmsTest, StableSortPreservesRelativeOrderForEqualKeys)
{
    SortItem items[] = {
        { 2, 0 },
        { 1, 1 },
        { 2, 2 },
        { 1, 3 },
        { 3, 4 },
        { 2, 5 },
    };

    stableSort(items, TF_ARRAY_COUNT(items), sizeof(SortItem), lessByKey, nullptr);

    EXPECT_EQ(items[0].key, 1);
    EXPECT_EQ(items[0].originalIndex, 1);
    EXPECT_EQ(items[1].key, 1);
    EXPECT_EQ(items[1].originalIndex, 3);
    EXPECT_EQ(items[2].key, 2);
    EXPECT_EQ(items[2].originalIndex, 0);
    EXPECT_EQ(items[3].key, 2);
    EXPECT_EQ(items[3].originalIndex, 2);
    EXPECT_EQ(items[4].key, 2);
    EXPECT_EQ(items[4].originalIndex, 5);
    EXPECT_EQ(items[5].key, 3);
    EXPECT_EQ(items[5].originalIndex, 4);
}

// Verifies that partitionInt32 places the pivot in its final slot and partitions around it.
TEST(CoreAlgorithmsTest, PartitionInt32PlacesElementsAroundPivotValue)
{
    int32_t values[] = { 9, 1, 5, 3, 5, 8, 2, 7 };
    const int32_t pivotValue = values[2];

    const size_t pivotIndex = partitionInt32(values, 2, TF_ARRAY_COUNT(values));

    ASSERT_LT(pivotIndex, TF_ARRAY_COUNT(values));
    EXPECT_EQ(values[pivotIndex], pivotValue);

    for (size_t i = 0; i < pivotIndex; ++i)
    {
        EXPECT_LT(values[i], pivotValue);
    }

    for (size_t i = pivotIndex + 1; i < TF_ARRAY_COUNT(values); ++i)
    {
        EXPECT_GE(values[i], pivotValue);
    }
}

// Verifies that the generic comparator-based sort produces nondecreasing keys.
TEST(CoreAlgorithmsTest, GenericSortOrdersByComparator)
{
    SortItem items[] = {
        { 4, 0 },
        { 2, 1 },
        { 5, 2 },
        { 1, 3 },
        { 3, 4 },
        { 2, 5 },
    };

    sort(items, TF_ARRAY_COUNT(items), sizeof(SortItem), lessByKey, nullptr);

    for (size_t i = 1; i < TF_ARRAY_COUNT(items); ++i)
    {
        EXPECT_LE(items[i - 1].key, items[i].key);
    }
}
