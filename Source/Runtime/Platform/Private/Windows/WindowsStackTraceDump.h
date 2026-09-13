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

#pragma once
#include "Core/IConfig.h"

#include "Platform/IOperatingSystem.h"
#include "Core/IThread.h"

struct WindowsStackTraceLineInfo
{
    char  functionName[512];
    char  moduleName[512];
    char  fileName[512];
    DWORD lineNumber;
};

class WindowsStackTrace
{
public:
    static bool Init();
    static void Exit();
    static LONG Dump(EXCEPTION_POINTERS* pExceptionInfo);

private:
#ifdef ENABLE_FORGE_STACKTRACE_DUMP
    static bool         init;
    static Mutex        dbgHelpMutex;
    static const size_t preallocatedMemorySize = 1024LL * 1024LL;
    static uint8_t      preallocatedMemory[preallocatedMemorySize];
    static size_t       usedMemorySize;

    static void* Alloc(size_t size);
    static void  Log(const char* msg, ...);
#endif // ENABLE_FORGE_STACKTRACE_DUMP
};
