#include <gtest/gtest.h>

#include "Core/ITime.h"
#include "Core/IThread.h"

// Verifies timer frequency, monotonic elapsed time, and Reset semantics for the basic timer helpers.
TEST(CoreTimeTest, TimerFunctionsReportMonotonicElapsedTime)
{
    EXPECT_GT(GetTimerFrequency(), 0);

    const int64_t start = GetUSec(true);
    ThreadSleep(15);
    const int64_t end = GetUSec(true);
    EXPECT_GE(end, start);
    EXPECT_GE(end - start, 5000);

    Timer timer = {};
    InitTimer(&timer);
    ThreadSleep(15);
    const uint32_t elapsedBeforeReset = GetTimerMSec(&timer, false);
    EXPECT_GE(elapsedBeforeReset, 5u);

    const uint32_t elapsedAtReset = GetTimerMSec(&timer, true);
    EXPECT_GE(elapsedAtReset, 5u);

    const uint32_t elapsedAfterReset = GetTimerMSec(&timer, false);
    EXPECT_LE(elapsedAfterReset, elapsedAtReset);
}

// Verifies that the high-resolution timer records multiple samples and reports a positive running average.
TEST(CoreTimeTest, HiresTimerTracksHistoryAndAverage)
{
    HiresTimer timer = {};
    InitHiresTimer(&timer);

    ThreadSleep(12);
    const int64_t first = GetHiresTimerUSec(&timer, true);
    EXPECT_GE(first, 5000);

    ThreadSleep(8);
    const int64_t second = GetHiresTimerUSec(&timer, true);
    EXPECT_GE(second, 3000);

    const int64_t average = GetHiresTimerUSecAverage(&timer);
    EXPECT_GT(average, 0);
    EXPECT_GT(GetHiresTimerSecondsAverage(&timer), 0.0f);
}
