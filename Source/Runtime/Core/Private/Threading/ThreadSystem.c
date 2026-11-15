/*
 * Copyright (c) 2017-2024 The Forge Interactive Inc.
 *
 * This file is part of The-Forge
 * (see https://github.com/ConfettiFX/The-Forge).
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include "ThreadSystem.h"

#include <ThirdParty/stb/stb_ds.h>

#include "Core/ILog.h"
#include "Core/IThread.h"
#include "Core/ITime.h"

#include "Atomics.h"

#define OPTIMAL_TASK_SLOTS_COUNT 128

struct ThreadSystemTask
{
    TaskFunc func;
    void*    user;
};

struct ThreadSystemData
{
    Mutex mutex;

    // const
    const char* name;
    uint64_t    threadCount;

    // [threadCount]
    ThreadHandle* threads;

    // Protected by mutex
    // TODO optimize queued task IO
    struct ThreadSystemTask* tasks;
    uint64_t                 tasksTaken;
    uint64_t                 tasksQueued;
    ConditionVariable        conditionTasks;
    ConditionVariable        conditionIsIdle;
    tfrg_atomic32_t          activatedThreadCount_Atomic;
    uint32_t                 idleThreadCount;
    //

    tfrg_atomic32_t references_Atomic;

    bool stopAbandon; // stop even if tasks are scheduled
    bool stop;
};

static void threadSystemCleanup(struct ThreadSystemData* t)
{
    ASSERT(tfrg_atomic32_load_relaxed(&t->references_Atomic) == 0);

    DestroyMutex(&t->mutex);
    DestroyConditionVariable(&t->conditionTasks);
    DestroyConditionVariable(&t->conditionIsIdle);

    arrfree(t->tasks);
    tf_free(t);
}

static inline void acquireThreadSystemHandle(struct ThreadSystemData* t) { tfrg_atomic32_add_relaxed(&t->references_Atomic, 1); }

static inline void releaseThreadSystemHandle(struct ThreadSystemData* t)
{
    uint64_t threadCount = tfrg_atomic32_add_relaxed(&t->references_Atomic, -1);
    if (threadCount == 1)
        threadSystemCleanup(t);
}

static struct ThreadSystemTask getTask(struct ThreadSystemData* t, uint64_t tid)
{
    struct ThreadSystemTask task = { 0 };

    if (t->stopAbandon)
        return task;

    LockMutex(&t->mutex);

    bool idleSet = false;

    for (;;)
    {
        if (t->tasksTaken < t->tasksQueued)
        {
            task = t->tasks[t->tasksTaken++];
            break;
        }

        if (t->stop)
            break;

        if (!idleSet && tid != UINT64_MAX)
        {
            idleSet = true;
            ++t->idleThreadCount;
        }
        BroadcastConditionVariable(&t->conditionIsIdle);
        WaitConditionVariable(&t->conditionTasks, &t->mutex, TIMEOUT_INFINITE);
    }

    if (idleSet)
        --t->idleThreadCount;

    uint64_t scheduledCount = t->tasksQueued - t->tasksTaken;
    if (t->tasksTaken > scheduledCount * 3)
    {
        if (scheduledCount)
        {
            memcpy(t->tasks, t->tasks + t->tasksTaken, scheduledCount * sizeof task); //-V595
        }

        t->tasksQueued -= t->tasksTaken;
        t->tasksTaken = 0;
    }

    size_t arrayLimit = arrlenu(t->tasks); //-V595
    if (arrayLimit > OPTIMAL_TASK_SLOTS_COUNT * 2)
        arrsetlen(t->tasks, OPTIMAL_TASK_SLOTS_COUNT);

    UnlockMutex(&t->mutex);

    return task;
}

static void taskThreadFunc(void* threadUserData)
{
    struct ThreadSystemData* t = threadUserData;

    uint64_t tid = tfrg_atomic32_add_relaxed(&t->activatedThreadCount_Atomic, 1);

    {
        char buffer[MAX_THREAD_NAME_LENGTH];
        snprintf(buffer, MAX_THREAD_NAME_LENGTH, "%s %llu", t->name, (unsigned long long)tid);
        SetCurrentThreadName(buffer);
    }

    struct ThreadSystemTask task = { 0 };
    while (!t->stopAbandon)
    {
        if (task.func)
        {
            task.func(task.user, tid);
            memset(&task, 0, sizeof task);
        }

        task = getTask(t, tid);
        if (t->stop && !task.func)
            break;
    }

    releaseThreadSystemHandle(t);
}

bool threadSystemInit(ThreadSystem* out, const struct ThreadSystemInitDesc* desc)
{
    *out = NULL;
    if (desc->threadCount == 0) // dummy run
        return true;

    uint64_t cpuCount = GetNumCpuCores();
    uint64_t count = desc->threadCount;

    if (count > cpuCount)
        count = cpuCount;

    if (count == 0) // something went wrong (maybe GetNumCpuCores returned 0)
        return false;

    struct ThreadSystemData* t = tf_calloc(1, sizeof(*t) + sizeof(ThreadHandle) * count);
    if (!t)
        return false;

    t->threads = (ThreadHandle*)(t + 1);
    t->name = desc->threadName ? desc->threadName : "ThreadSystem";

    bool success = false;

    do
    {
        if (!InitMutex(&t->mutex))
        {
            memset(&t->mutex, 0, sizeof t->mutex);
            break;
        }

        if (!InitConditionVariable(&t->conditionTasks))
        {
            memset(&t->conditionTasks, 0, sizeof t->conditionTasks);
            break;
        }

        if (!InitConditionVariable(&t->conditionIsIdle))
        {
            memset(&t->conditionIsIdle, 0, sizeof t->conditionIsIdle);
            break;
        }

        success = true;
    } while (false);

    if (!success)
    {
        threadSystemCleanup(t);
    }

    ThreadDesc threadDesc = { 0 };

    threadDesc.pFunc = taskThreadFunc;
    threadDesc.pData = t;

#if defined(_WINDOWS) // for some reason on Windows thread name won't change after creation
    strncpy(threadDesc.threadName, t->name, sizeof threadDesc.threadName);
    threadDesc.threadName[sizeof(threadDesc.threadName) - 1] = 0;
#endif

    if (desc->setAffinityMask)
    {
        threadDesc.setAffinityMask = true;
        memcpy(threadDesc.affinityMask, desc->affinityMask, sizeof threadDesc.affinityMask);
    }

    arrsetlen(t->tasks, OPTIMAL_TASK_SLOTS_COUNT);

    t->threadCount = count;

    for (uint64_t ti = 0; ti < count; ++ti)
    {
        acquireThreadSystemHandle(t);

        if (InitThread(&threadDesc, t->threads + ti))
            continue;

        t->stop = true;
        t->stopAbandon = true;

        releaseThreadSystemHandle(t);
        return false;
    }

    acquireThreadSystemHandle(t);
    *out = t;
    return true;
}

void threadSystemExit(ThreadSystem* thandle, const struct ThreadSystemExitDesc* desc)
{
    struct ThreadSystemData* t = *thandle;
    if (!t)
        return;
    *thandle = NULL;

    LockMutex(&t->mutex);
    t->stop = true;
    if (desc->abandonTasks)
        t->stopAbandon = true;
    BroadcastConditionVariable(&t->conditionTasks);
    UnlockMutex(&t->mutex);

    if (!desc->detachThreads)
    {
        for (uint64_t ti = 0; ti < t->threadCount; ++ti)
            JoinThread(t->threads[ti]);
    }
    else
    {
        for (uint64_t ti = 0; ti < t->threadCount; ++ti)
            DetachThread(t->threads[ti]);
    }

    releaseThreadSystemHandle(t);
}

void threadSystemAddTasks(ThreadSystem thandle, TaskFunc func, uint64_t count, uint64_t userSize, void* users)
{
    if (count == 0)
        return;
    if (!VERIFY(func))
        return;

    struct ThreadSystemData* t = thandle;

    if (!t) // dummy run
    {
        for (uint64_t ti = 0; ti < count; ++ti)
            func((uint8_t*)users + ti * userSize, 0);
        return;
    }

    LockMutex(&t->mutex);

    uint64_t offset = t->tasksQueued;

    t->tasksQueued += count;

    uint64_t len = arrlenu(t->tasks);

    if (t->tasksQueued > len)
    {
        // Resize the task array to a multiple of OPTIMAL_TASK_SLOTS_COUNT that is large enough to contain all of the requested tasks.
        uint64_t newTasksLength = t->tasksQueued / OPTIMAL_TASK_SLOTS_COUNT;
        newTasksLength += (t->tasksQueued % OPTIMAL_TASK_SLOTS_COUNT) == 0 ? 0 : 1;
        newTasksLength *= OPTIMAL_TASK_SLOTS_COUNT;
        arrsetlen(t->tasks, newTasksLength);
    }

    for (uint64_t ti = 0; ti < count; ++ti)
    {
        t->tasks[offset + ti] = (struct ThreadSystemTask){
            func,
            users ? ((uint8_t*)users + ti * userSize) : NULL,
        };
    }

    if (count == 1)
        SignalConditionVariable(&t->conditionTasks);
    else
        BroadcastConditionVariable(&t->conditionTasks);

    UnlockMutex(&t->mutex);
    return;
}

bool threadSystemAssist(ThreadSystem thandle)
{
    struct ThreadSystemData* t = thandle;
    if (!t)
        return false;

    struct ThreadSystemTask task = getTask(t, UINT64_MAX);
    if (task.func)
        task.func(task.user, UINT64_MAX);
    return task.func;
}

bool threadSystemWaitIdleTimeout(ThreadSystem thandle, uint32_t timeout_ms)
{
    struct ThreadSystemData* t = thandle;
    if (!t)
        return true;

    Timer timer;
    InitTimer(&timer);

    bool idle = false;
    LockMutex(&t->mutex);
    for (;;)
    {
        idle = (t->tasksTaken >= t->tasksQueued) && (t->idleThreadCount >= tfrg_atomic32_load_relaxed(&t->references_Atomic) - 1);
        if (idle || timeout_ms == 0)
            break;

        if (timeout_ms != UINT32_MAX)
        {
            uint32_t ms = GetTimerMSec(&timer, false);
            if (ms > timeout_ms)
                break;
            WaitConditionVariable(&t->conditionIsIdle, &t->mutex, timeout_ms - ms);
        }
        else
        {
            WaitConditionVariable(&t->conditionIsIdle, &t->mutex, TIMEOUT_INFINITE);
        }
    }
    UnlockMutex(&t->mutex);
    return idle;
}

void threadSystemGetInfo(ThreadSystem thandle, struct ThreadSystemInfo* outInfo)
{
    memset(outInfo, 0, sizeof *outInfo);

    struct ThreadSystemData* t = thandle;
    if (!t)
        return;

    outInfo->threadCount = t->threadCount;
    outInfo->executedThreadCount = tfrg_atomic32_load_relaxed(&t->activatedThreadCount_Atomic);
    outInfo->activeThreadCount = tfrg_atomic32_load_relaxed(&t->references_Atomic) - 1;
    outInfo->threadName = t->name;
}
