#include <gtest/gtest.h>

#include <string.h>

#include "Core/IRandom.h"

// Verifies that random integers stay within the advertised range and do not degenerate into a constant sample.
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

TEST(CoreRandomTest, SystemRandomBytesRespectRangeAndVary)
{
    uint8_t bytes[35];
    memset(bytes, 0xa5, sizeof(bytes));
    ASSERT_TRUE(hz::getSystemRandomBytes(bytes + 1, 33));
    EXPECT_EQ(bytes[0], 0xa5);
    EXPECT_EQ(bytes[34], 0xa5);

    uint8_t second[33] = {};
    ASSERT_TRUE(hz::getSystemRandomBytes(second, sizeof(second)));
    EXPECT_NE(memcmp(bytes + 1, second, sizeof(second)), 0);
    EXPECT_TRUE(hz::getSystemRandomBytes(nullptr, 0));
    EXPECT_TRUE(hz::getSystemRandomBytes(bytes, 0));
    EXPECT_EQ(bytes[0], 0xa5);
}
