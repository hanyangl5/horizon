#include <gtest/gtest.h>

#include <string.h>

#include "Core/IThread.h"

namespace
{
int* gCallOnceCounter = nullptr;

void incrementCallOnceCounter()
{
    ++(*gCallOnceCounter);
}

struct CallOnceWorkerData
{
    CallOnceGuard* guard;
};

void callOnceWorker(void* userData)
{
    auto* data = (CallOnceWorkerData*)userData;
    callOnce(data->guard, incrementCallOnceCounter);
}

struct ThreadProbeData
{
    char observedName[MAX_THREAD_NAME_LENGTH + 1] = {};
    ThreadID observedThreadId = INVALID_THREAD_ID;
    bool observedIsMainThread = true;

    Mutex* mutex = nullptr;
    ConditionVariable* condition = nullptr;
    bool* ready = nullptr;
};

void namedThreadProbe(void* userData)
{
    auto* data = (ThreadProbeData*)userData;
    data->observedThreadId = getCurrentThreadID();
    data->observedIsMainThread = isMainThread();
    getCurrentThreadName(data->observedName, TF_ARRAY_COUNT(data->observedName));

    acquireMutex(data->mutex);
    *data->ready = true;
    wakeOneConditionVariable(data->condition);
    releaseMutex(data->mutex);
}
} // namespace

// Verifies that callOnce executes the guarded function exactly once even when multiple threads race to invoke it.
TEST(CoreThreadTest, CallOnceInvokesFunctionSingleTimeAcrossThreads)
{
    int callCount = 0;
    gCallOnceCounter = &callCount;

    CallOnceGuard guard = INIT_CALL_ONCE_GUARD;
    CallOnceWorkerData workerData[4] = {};
    ThreadHandle handles[4] = {};

    for (size_t i = 0; i < TF_ARRAY_COUNT(workerData); ++i)
    {
        workerData[i].guard = &guard;
    }

    for (size_t i = 0; i < TF_ARRAY_COUNT(handles); ++i)
    {
        ThreadDesc desc = {
            .pFunc = callOnceWorker,
            .pData = &workerData[i],
        };
        ASSERT_TRUE(initThread(&desc, &handles[i]));
    }

    for (size_t i = 0; i < TF_ARRAY_COUNT(handles); ++i)
    {
        joinThread(handles[i]);
    }

    EXPECT_EQ(callCount, 1);
    gCallOnceCounter = nullptr;
}

// Verifies that worker threads expose their assigned name and can coordinate readiness with a condition variable.
TEST(CoreThreadTest, ThreadNamingAndConditionVariableWorkTogether)
{
    setMainThread();
    EXPECT_TRUE(isMainThread());

    Mutex mutex = {};
    ConditionVariable condition = {};
    ASSERT_TRUE(initMutex(&mutex));
    ASSERT_TRUE(initConditionVariable(&condition));

    bool ready = false;
    ThreadProbeData data = {
        .mutex = &mutex,
        .condition = &condition,
        .ready = &ready,
    };

    ThreadDesc desc = {
        .pFunc = namedThreadProbe,
        .pData = &data,
    };
    memcpy(desc.mThreadName, "CoreThreadUT", sizeof("CoreThreadUT"));

    ThreadHandle handle = nullptr;
    ASSERT_TRUE(initThread(&desc, &handle));

    acquireMutex(&mutex);
    while (!ready)
    {
        waitConditionVariable(&condition, &mutex, TIMEOUT_INFINITE);
    }
    releaseMutex(&mutex);

    joinThread(handle);

    EXPECT_STREQ(data.observedName, "CoreThreadUT");
    EXPECT_NE(data.observedThreadId, INVALID_THREAD_ID);
    EXPECT_FALSE(data.observedIsMainThread);
    EXPECT_GT(getNumCPUCores(), 0u);

    destroyConditionVariable(&condition);
    destroyMutex(&mutex);
}
