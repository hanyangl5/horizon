#include <gtest/gtest.h>

#include "Core/ITime.h"

// Verifies that int64MulDiv preserves expected scaling behavior for representative integer inputs.
TEST(CoreMathTest, Int64MulDivScalesValuesWithoutLosingRemainderBehavior)
{
    EXPECT_EQ(int64MulDiv(42, 7, 3), 98);
    EXPECT_EQ(int64MulDiv(1234567890123LL, 1000, 1000000), 1234567890LL);
    EXPECT_EQ(int64MulDiv(9876543210LL, 3, 2), 14814814815LL);
}
