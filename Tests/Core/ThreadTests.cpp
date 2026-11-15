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
    CallOnce(data->guard, incrementCallOnceCounter);
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
    data->observedThreadId = GetCurrentThreadID();
    data->observedIsMainThread = IsMainThread();
    GetCurrentThreadName(data->observedName, TF_ARRAY_COUNT(data->observedName));

    LockMutex(data->mutex);
    *data->ready = true;
    SignalConditionVariable(data->condition);
    UnlockMutex(data->mutex);
}
} // namespace

// Verifies that CallOnce executes the guarded function exactly once even when multiple threads race to invoke it.
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
            .Func = callOnceWorker,
            .Data = &workerData[i],
        };
        ASSERT_TRUE(InitThread(&desc, &handles[i]));
    }

    for (size_t i = 0; i < TF_ARRAY_COUNT(handles); ++i)
    {
        JoinThread(handles[i]);
    }

    EXPECT_EQ(callCount, 1);
    gCallOnceCounter = nullptr;
}

// Verifies that worker threads expose their assigned name and can coordinate readiness with a condition variable.
TEST(CoreThreadTest, ThreadNamingAndConditionVariableWorkTogether)
{
    SetMainThread();
    EXPECT_TRUE(IsMainThread());

    Mutex mutex = {};
    ConditionVariable condition = {};
    ASSERT_TRUE(InitMutex(&mutex));
    ASSERT_TRUE(InitConditionVariable(&condition));

    bool ready = false;
    ThreadProbeData data = {
        .mutex = &mutex,
        .condition = &condition,
        .ready = &ready,
    };

    ThreadDesc desc = {
        .Func = namedThreadProbe,
        .Data = &data,
    };
    memcpy(desc.threadName, "CoreThreadUT", sizeof("CoreThreadUT"));

    ThreadHandle handle = nullptr;
    ASSERT_TRUE(InitThread(&desc, &handle));

    LockMutex(&mutex);
    while (!ready)
    {
        WaitConditionVariable(&condition, &mutex, TIMEOUT_INFINITE);
    }
    UnlockMutex(&mutex);

    JoinThread(handle);

    EXPECT_STREQ(data.observedName, "CoreThreadUT");
    EXPECT_NE(data.observedThreadId, INVALID_THREAD_ID);
    EXPECT_FALSE(data.observedIsMainThread);
    EXPECT_GT(GetNumCpuCores(), 0u);

    DestroyConditionVariable(&condition);
    DestroyMutex(&mutex);
}
