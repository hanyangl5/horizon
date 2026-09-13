#include <gtest/gtest.h>

#include "Core/IMath.h"
#include "Core/ITime.h"

TEST(CoreMathTest, Matrix4ScalarConstructorUsesColumnMajorOrder)
{
    const mat4    matrix(1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f, 11.0f, 12.0f, 13.0f, 14.0f, 15.0f, 16.0f);
    const Vector4 result = matrix * Vector4(1.0f, 2.0f, 3.0f, 1.0f);
    EXPECT_FLOAT_EQ((float)result.getX(), 51.0f);
    EXPECT_FLOAT_EQ((float)result.getY(), 58.0f);
    EXPECT_FLOAT_EQ((float)result.getZ(), 65.0f);
    EXPECT_FLOAT_EQ((float)result.getW(), 72.0f);
}

// Verifies that int64MulDiv preserves expected scaling behavior for representative integer inputs.
TEST(CoreMathTest, Int64MulDivScalesValuesWithoutLosingRemainderBehavior)
{
    EXPECT_EQ(int64MulDiv(42, 7, 3), 98);
    EXPECT_EQ(int64MulDiv(1234567890123LL, 1000, 1000000), 1234567890LL);
    EXPECT_EQ(int64MulDiv(9876543210LL, 3, 2), 14814814815LL);
}
