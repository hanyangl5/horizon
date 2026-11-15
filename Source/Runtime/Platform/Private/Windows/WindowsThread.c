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

#include "Core/IConfig.h"

#if defined(_WINDOWS) || defined(XBOX)

#include "Core/ILog.h"
#include "Core/IThread.h"
#include "Platform/IOperatingSystem.h"

#include "Core/IMemory.h"

#if defined(ENABLE_PROFILER)
#include <tracy/TracyC.h>
#endif

#if defined(ENABLE_THREAD_PERFORMANCE_STATS)

#if defined(XBOX)
#include <processthreadsapi.h>

int      gThreadIds[MAX_PERFORMANCE_STATS_CORES];
uint64_t gThreadWorkTimes[MAX_PERFORMANCE_STATS_CORES];
uint64_t gPrevThreadWorkTimes[MAX_PERFORMANCE_STATS_CORES];
uint64_t gLastSamplingTime;
Mutex    gThreadTimesMutex;

#else

#include <Wbemidl.h>
#pragma comment(lib, "wbemuuid.lib")

IWbemLocator*  gWbemLocator = NULL;
IWbemServices* gWbemService = NULL;
uint64_t       pOldTimeStamp[MAX_PERFORMANCE_STATS_CORES];
uint64_t       pOldPprocUsage[MAX_PERFORMANCE_STATS_CORES];

#endif

#endif // ENABLE_THREAD_PERFORMANCE_STATS

#include <process.h> // _beginthreadex

/*
 * Function pointer can't be casted to void*,
 * so we wrap it with a struct
 */
typedef struct CallOnceFnWrapper
{
    CallOnceFn fn;
} CallOnceFnWrapper;

static BOOL callOnceImpl(MAYBE_UNUSED PINIT_ONCE initOnce, PVOID pWrapper, MAYBE_UNUSED PVOID* ppContext)
{
    // UNREF_PARAM(initOnce);
    // UNREF_PARAM(ppContext);
    CallOnceFn fn = ((CallOnceFnWrapper*)pWrapper)->fn;
    if (fn)
        fn();

    return TRUE;
}

void CallOnce(CallOnceGuard* pGuard, CallOnceFn fn)
{
    CallOnceFnWrapper wrapper = { fn };
    InitOnceExecuteOnce(pGuard, callOnceImpl, &wrapper, NULL);
}

bool InitMutex(Mutex* mutex)
{
    return InitializeCriticalSectionAndSpinCount((CRITICAL_SECTION*)&mutex->handle, (DWORD)MUTEX_DEFAULT_SPIN_COUNT);
}

void DestroyMutex(Mutex* mutex)
{
    CRITICAL_SECTION* cs = (CRITICAL_SECTION*)&mutex->handle;
    DeleteCriticalSection(cs);
    memset(&mutex->handle, 0, sizeof(mutex->handle));
}

void LockMutex(Mutex* mutex) { EnterCriticalSection((CRITICAL_SECTION*)&mutex->handle); }

bool TryLockMutex(Mutex* mutex) { return TryEnterCriticalSection((CRITICAL_SECTION*)&mutex->handle); }

void UnlockMutex(Mutex* mutex) { LeaveCriticalSection((CRITICAL_SECTION*)&mutex->handle); }

bool InitConditionVariable(ConditionVariable* cv)
{
    cv->pHandle = (CONDITION_VARIABLE*)MemCallocate(1, sizeof(CONDITION_VARIABLE));
    InitializeConditionVariable((PCONDITION_VARIABLE)cv->pHandle);
    return true;
}

void DestroyConditionVariable(ConditionVariable* cv) { MemDeallocate(cv->pHandle); }

void WaitConditionVariable(ConditionVariable* cv, Mutex* pMutex, uint32_t ms)
{
    SleepConditionVariableCS((PCONDITION_VARIABLE)cv->pHandle, (PCRITICAL_SECTION)&pMutex->handle, ms);
}

void SignalConditionVariable(ConditionVariable* cv) { WakeConditionVariable((PCONDITION_VARIABLE)cv->pHandle); }

void BroadcastConditionVariable(ConditionVariable* cv) { WakeAllConditionVariable((PCONDITION_VARIABLE)cv->pHandle); }

static ThreadID mainThreadID = 0;

void SetMainThread() { mainThreadID = GetCurrentThreadID(); }

ThreadID GetCurrentThreadID() { return GetCurrentThreadId(); /* Windows built-in function*/ }

char* thread_name()
{
    __declspec(thread) static char name[MAX_THREAD_NAME_LENGTH + 1];
    return name;
}

void GetCurrentThreadName(char* buffer, int size)
{
    const char* name = thread_name();
    if (name[0])
        snprintf(buffer, (size_t)size, "%s", name);
    else
        buffer[0] = 0;
}

void SetCurrentThreadName(const char* name)
{
    strcpy_s(thread_name(), MAX_THREAD_NAME_LENGTH + 1, name);
#if defined(ENABLE_PROFILER)
    TracyCSetThreadName(name);
#endif
}

bool IsMainThread() { return GetCurrentThreadID() == mainThreadID; }

typedef int(__cdecl* SETTHREADDESCFUNC)(HANDLE, PCWSTR);

unsigned WINAPI ThreadFunctionStatic(void* data)
{
    ThreadDesc item = *((ThreadDesc*)(data));
    MemDeallocate(data);

    if (item.threadName[0] != 0)
    {
        // Local TheForge thread name, used for logging
        SetCurrentThreadName(item.threadName);

#ifdef _WINDOWS
        HINSTANCE hinstLib = GetModuleHandle(TEXT("KernelBase.dll"));
        if (hinstLib != NULL)
        {
            SETTHREADDESCFUNC ProcAdd = (SETTHREADDESCFUNC)GetProcAddress(hinstLib, "SetThreadDescription");
            if (ProcAdd != NULL)
            {
                WCHAR windowsThreadName[sizeof(item.threadName)] = { 0 };
                mbstowcs(windowsThreadName, item.threadName, strlen(item.threadName) + 1);
                HRESULT res = ProcAdd(GetCurrentThread(), windowsThreadName);
                ASSERT(!FAILED(res));
            }
        }
#endif
    }

    if (item.setAffinityMask)
    {
        uint32_t nCores = GetNumCpuCores();

        for (uint32_t groupId = 0; groupId < nCores / 64; ++groupId)
        {
            GROUP_AFFINITY groupAffinity = { 0 };

            groupAffinity.Mask = item.affinityMask[groupId];
            groupAffinity.Group = (WORD)groupId;

            BOOL res = SetThreadGroupAffinity(GetCurrentThread(), &groupAffinity, NULL);
            if (res != 0)
            {
                LOGF(Error, "Failed to set affinity for thread %s for CPU group %u: 0x%x", item.threadName, groupId, res);
            }
        }
    }

    item.pFunc(item.pData);
    return 0;
}

void ThreadSleep(unsigned mSec) { Sleep(mSec); }

bool InitThread(ThreadDesc* pDesc, ThreadHandle* pHandle)
{
    ASSERT(pHandle);
    *pHandle = NULL;

    // Copy the contents of ThreadDesc because if the variable is in the stack we might access corrupted data.
    ThreadDesc* pDescCopy = (ThreadDesc*)MemAllocate(sizeof(ThreadDesc));
    *pDescCopy = *pDesc;

    *pHandle = (ThreadHandle)_beginthreadex(0, 0, ThreadFunctionStatic, pDescCopy, 0, 0);
    return *pHandle != NULL;
}

void JoinThread(ThreadHandle handle)
{
    if (!handle)
        return;
    WaitForSingleObject((HANDLE)handle, INFINITE);
    CloseHandle((HANDLE)handle);
}

void DetachThread(ThreadHandle handle)
{
    if (!handle)
        return;
    CloseHandle((HANDLE)handle);
}

unsigned int GetNumCpuCores(void)
{
    struct _SYSTEM_INFO systemInfo;
    GetSystemInfo(&systemInfo);
    return systemInfo.dwNumberOfProcessors;
}

// Performance stats functions
#if defined(ENABLE_THREAD_PERFORMANCE_STATS)

int InitPerformanceStats(PerformanceStatsFlags flags)
{
    UNREF_PARAM(flags);
#if defined(XBOX)
    for (uint32_t i = 0; i < GetNumCpuCores(); ++i)
    {
        gThreadIds[i] = -1;
        gThreadWorkTimes[i] = 0;
        gPrevThreadWorkTimes[i] = 0;
    }

    FILETIME create, exit, user, kernel;
    GetProcessTimes(GetCurrentProcess(), &create, &exit, &kernel, &user);
    uint64_t lowPart = create.dwLowDateTime;
    uint64_t highPart = create.dwHighDateTime;
    uint64_t creationTime = lowPart | (highPart << 32uLL);

    gLastSamplingTime = creationTime;

    InitMutex(&gThreadTimesMutex);

#else
    HRESULT hres;

    CoInitializeEx(0, COINIT_MULTITHREADED);
    CoInitializeSecurity(NULL, -1, NULL, NULL, RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE, NULL);

    FILETIME currSysTime;
    GetSystemTimeAsFileTime(&currSysTime);
    ULARGE_INTEGER uli;
    uli.LowPart = currSysTime.dwLowDateTime;
    uli.HighPart = currSysTime.dwHighDateTime;

    for (uint32_t i = 0; i < GetNumCpuCores(); i++)
    {
        pOldTimeStamp[i] = uli.QuadPart;
        pOldPprocUsage[i] = 0;
    }

    hres = CoCreateInstance(&CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, &IID_IWbemLocator, (LPVOID*)&gWbemLocator);
    if (FAILED(hres))
    {
        CoUninitialize();
        return -1;
    }

    BSTR resString = SysAllocString(L"ROOT\\CIMV2");
    hres = gWbemLocator->lpVtbl->ConnectServer(gWbemLocator, resString, NULL, NULL, NULL, 0, NULL, NULL, &gWbemService);
    SysFreeString(resString);

    if (FAILED(hres))
    {
        gWbemLocator->lpVtbl->Release(gWbemLocator);
        CoUninitialize();
        return -1;
    }

#endif
    return 1;
}

void UpdatePerformanceStats()
{
#if defined(XBOX)
    // Update current thread times
    HANDLE currThread = GetCurrentThread();
    DWORD  threadId = GetCurrentThreadID();
    DWORD  coreId = GetCurrentProcessorNumber();

    FILETIME creation, exit, user, kernel;
    GetThreadTimes(currThread, &creation, &exit, &kernel, &user);

    uint32_t threadIdx = (uint32_t)-1;
    for (uint32_t i = 0; i < GetNumCpuCores(); i++)
    {
        if (gThreadIds[i] == (int)threadId)
        {
            threadIdx = i;
            break;
        }
        else if (gThreadIds[i] == -1)
        {
            gThreadIds[i] = threadId;
            threadIdx = i;
            break;
        }
    }
    ASSERT(threadIdx != -1);

    {
        LockMutex(&gThreadTimesMutex);

        uint64_t lowPart = (uint64_t)user.dwLowDateTime;
        uint64_t highPart = (uint64_t)user.dwHighDateTime;
        uint64_t userTime = (lowPart | (highPart << 32uLL));

        lowPart = (uint64_t)kernel.dwLowDateTime;
        highPart = (uint64_t)kernel.dwHighDateTime;
        uint64_t kernelTime = (lowPart | (highPart << 32uLL));
        // Add time elapsed since last sampling
        gThreadWorkTimes[coreId] += (kernelTime + userTime) - gPrevThreadWorkTimes[threadIdx];

        // Update total user / kernel times
        gPrevThreadWorkTimes[threadIdx] = userTime + kernelTime;

        UnlockMutex(&gThreadTimesMutex);
    }
#endif
}

PerformanceStats GetPerformanceStats()
{
    PerformanceStats ret = { { 0 } };
    for (uint32_t i = 0; i < GetNumCpuCores(); i++)
        ret.coreUsagePercentage[i] = -1;

#if defined(XBOX)
    FILETIME currTime;
    GetSystemTimeAsFileTime(&currTime);

    uint64_t lowPart = currTime.dwLowDateTime;
    uint64_t highPart = currTime.dwHighDateTime;
    uint64_t processTime = lowPart | (highPart << 32uLL);

    // Time elapsed since last sampling
    uint64_t elapsedTime = processTime - gLastSamplingTime;
    {
        LockMutex(&gThreadTimesMutex);

        for (uint32_t i = 0; i < GetNumCpuCores(); ++i)
        {
            ret.coreUsagePercentage[i] = (100.0f * ((float)gThreadWorkTimes[i] / elapsedTime));
            gThreadWorkTimes[i] = 0;
        }

        UnlockMutex(&gThreadTimesMutex);
    }

    gLastSamplingTime = processTime;
#else
    IEnumWbemClassObject* pEnumerator;
    BSTR                  wql = SysAllocString(L"WQL");
    BSTR                  query =
        SysAllocString(L"SELECT TimeStamp_Sys100NS, PercentProcessorTime, Frequency_PerfTime FROM Win32_PerfRawData_PerfOS_Processor");

    HRESULT hres = gWbemService->lpVtbl->ExecQuery(gWbemService, wql, query, WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL,
                                                   &pEnumerator);
    SysFreeString(wql);
    SysFreeString(query);

    if (FAILED(hres))
    {
        gWbemService->lpVtbl->Release(gWbemService);
        gWbemLocator->lpVtbl->Release(gWbemLocator);
        CoUninitialize();

        return ret;
    }

    IWbemClassObject* pclsObj = NULL;

    ULONG i = 0;
    ULONG retVal;

    while (pEnumerator)
    {
        // Waiting for inifinite blocks resources and app.
        // Waiting for 15 ms (arbitrary) instead works much better
        pEnumerator->lpVtbl->Next(pEnumerator, 15, 1, &pclsObj, &retVal);
        if (!retVal)
        {
            break;
        }

        VARIANT vtPropTime;
        VARIANT vtPropClock;
        VariantInit(&vtPropTime);
        VariantInit(&vtPropClock);

        BSTR timeStamp = SysAllocString(L"TimeStamp_Sys100NS");
        BSTR procTime = SysAllocString(L"PercentProcessorTime");

        pclsObj->lpVtbl->Get(pclsObj, timeStamp, 0, &vtPropTime, 0, 0);
        UINT64 newTimeStamp = _wtoi64(vtPropTime.bstrVal);

        pclsObj->lpVtbl->Get(pclsObj, procTime, 0, &vtPropClock, 0, 0);
        UINT64 newPProcUsage = _wtoi64(vtPropClock.bstrVal);

        SysFreeString(timeStamp);
        SysFreeString(procTime);

        ret.coreUsagePercentage[i] =
            (float)(1.0 - (((double)newPProcUsage - (double)pOldPprocUsage[i]) / ((double)newTimeStamp - (double)pOldTimeStamp[i]))) *
            100.0f;

        pOldPprocUsage[i] = newPProcUsage;
        pOldTimeStamp[i] = newTimeStamp;

        VariantClear(&vtPropTime);
        VariantClear(&vtPropClock);

        pclsObj->lpVtbl->Release(pclsObj);
        i++;
    }
#endif

    return ret;
}

void ExitPerformanceStats()
{
#if !defined(XBOX)
    gWbemService->lpVtbl->Release(gWbemService);
    gWbemLocator->lpVtbl->Release(gWbemLocator);
    CoUninitialize();
#endif
}
#endif // ENABLE_THREAD_PERFORMANCE_STATS
#endif
