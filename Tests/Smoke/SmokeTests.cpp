#include <gtest/gtest.h>

#include "Core/IConfig.h"

// Verifies that public runtime headers expose the expected basic configuration macros.
TEST(HorizonSmokeTest, RuntimePublicHeadersAreAvailable)
{
    constexpr int values[] = { 1, 2, 3 };

    EXPECT_EQ(TF_MIN(3, 7), 3);
    EXPECT_EQ(TF_MAX(3, 7), 7);
    EXPECT_EQ(TF_ARRAY_COUNT(values), 3);
}
