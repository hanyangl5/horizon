#include <gtest/gtest.h>

#include <stdio.h>
#include <string.h>

#include "Core/ILog.h"

namespace
{
constexpr size_t LOG_CAPTURE_CAPACITY = 8;
constexpr size_t LOG_MESSAGE_CAPACITY = 512;

struct LogCapture
{
    char messages[LOG_CAPTURE_CAPACITY][LOG_MESSAGE_CAPACITY];
    size_t messageCount = 0;
    int closeCount = 0;
};

void captureLog(void* userData, const char* message)
{
    LogCapture* capture = static_cast<LogCapture*>(userData);
    if (capture->messageCount >= LOG_CAPTURE_CAPACITY)
        return;

    snprintf(capture->messages[capture->messageCount], LOG_MESSAGE_CAPACITY, "%s", message);
    ++capture->messageCount;
}

void closeLogCapture(void* userData)
{
    ++static_cast<LogCapture*>(userData)->closeCount;
}

bool containsMessage(const LogCapture& capture, const char* snippet)
{
    for (size_t i = 0; i < capture.messageCount; ++i)
    {
        if (strstr(capture.messages[i], snippet) != nullptr)
            return true;
    }

    return false;
}
} // namespace

// Verifies that interactive logging mode can be toggled off and back on without losing state.
TEST(CoreLogTest, InteractiveModeToggleRoundTrips)
{
    const bool initial = _IsInteractiveMode();

    _EnableInteractiveMode(!initial);
    EXPECT_EQ(_IsInteractiveMode(), !initial);

    _EnableInteractiveMode(initial);
    EXPECT_EQ(_IsInteractiveMode(), initial);
}

// Verifies that log callbacks receive formatted and raw messages and are closed during shutdown.
TEST(CoreLogTest, CallbackReceivesFormattedAndRawMessages)
{
    LogCapture capture = {};

    initLog(nullptr, eALL);
    //setCurrentThreadName("CoreLogUT");
    addLogCallback("CoreLogTest.CallbackReceivesFormattedAndRawMessages", eALL, &capture, captureLog, closeLogCapture, nullptr);

    writeLog(eINFO, __FILE__, __LINE__, "hello %d", 42);
    writeRawLog(eWARNING, false, "raw warning");

    exitLog();

    EXPECT_GE(capture.messageCount, 2u);
    EXPECT_TRUE(containsMessage(capture, "INFO|"));
    EXPECT_TRUE(containsMessage(capture, "hello 42"));
    EXPECT_TRUE(containsMessage(capture, "raw warning"));
    EXPECT_EQ(capture.closeCount, 1);
}

// Verifies that human-readable log helpers format representative size and time values.
TEST(CoreLogTest, HumanReadableHelpersFormatExpectedUnits)
{
    EXPECT_STREQ(humanReadableSize(999).str, "999B");
    EXPECT_STREQ(humanReadableSize(1024).str, "1KB");
    EXPECT_STREQ(humanReadableSize(1536).str, "1.5KB");
    EXPECT_STRNE(humanReadableTime(500).str, "");
}
