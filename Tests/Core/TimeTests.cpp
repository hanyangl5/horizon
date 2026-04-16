#include <gtest/gtest.h>

#include "Core/ITime.h"
#include "Core/IThread.h"

TEST(CoreTimeTest, TimerFunctionsReportMonotonicElapsedTime)
{
    EXPECT_GT(getTimerFrequency(), 0);

    const int64_t start = getUSec(true);
    threadSleep(15);
    const int64_t end = getUSec(true);
    EXPECT_GE(end, start);
    EXPECT_GE(end - start, 5000);

    Timer timer = {};
    initTimer(&timer);
    threadSleep(15);
    const uint32_t elapsedBeforeReset = getTimerMSec(&timer, false);
    EXPECT_GE(elapsedBeforeReset, 5u);

    const uint32_t elapsedAtReset = getTimerMSec(&timer, true);
    EXPECT_GE(elapsedAtReset, 5u);

    const uint32_t elapsedAfterReset = getTimerMSec(&timer, false);
    EXPECT_LE(elapsedAfterReset, elapsedAtReset);
}

TEST(CoreTimeTest, HiresTimerTracksHistoryAndAverage)
{
    HiresTimer timer = {};
    initHiresTimer(&timer);

    threadSleep(12);
    const int64_t first = getHiresTimerUSec(&timer, true);
    EXPECT_GE(first, 5000);

    threadSleep(8);
    const int64_t second = getHiresTimerUSec(&timer, true);
    EXPECT_GE(second, 3000);

    const int64_t average = getHiresTimerUSecAverage(&timer);
    EXPECT_GT(average, 0);
    EXPECT_GT(getHiresTimerSecondsAverage(&timer), 0.0f);
}
