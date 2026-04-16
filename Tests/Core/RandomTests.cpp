#include <gtest/gtest.h>

#include "Core/IRandom.h"

TEST(CoreRandomTest, RandomValuesStayWithinExpectedRangeAndVary)
{
    int32_t values[16] = {};

    for (size_t i = 0; i < TF_ARRAY_COUNT(values); ++i)
    {
        values[i] = getRandomInt();
        EXPECT_GE(values[i], 0);
        EXPECT_LE(values[i], TF_RAND_MAX);
    }

    bool allSame = true;
    for (size_t i = 1; i < TF_ARRAY_COUNT(values); ++i)
    {
        if (values[i] != values[0])
        {
            allSame = false;
            break;
        }
    }

    EXPECT_FALSE(allSame);
}
