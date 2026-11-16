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

#include "Core/IFileSystem.h"

#include "stdbool.h"

#ifndef FILENAME_NAME_LENGTH_LOG
#define FILENAME_NAME_LENGTH_LOG 23
#endif

#ifndef INDENTATION_SIZE_LOG
#define INDENTATION_SIZE_LOG 4
#endif

#ifndef LEVELS_LOG
#define LEVELS_LOG 6
#endif

#define CONCAT_STR_LOG_IMPL(a, b) a##b
#define CONCAT_STR_LOG(a, b)      CONCAT_STR_LOG_IMPL(a, b)

#ifndef ANONIMOUS_VARIABLE_LOG
#define ANONIMOUS_VARIABLE_LOG(str) CONCAT_STR_LOG(str, __LINE__)
#endif

// If you Add more levels don't forget to change LOG_LEVELS macro to the actual number of levels
typedef enum LogLevel
{
    None = 0,
    Raw = 1,
    Debug = 2,
    Info = 4,
    Warning = 8,
    Error = 16,
    All = ~0
} LogLevel;

typedef void (*LogCallbackFn)(void* user_data, const char* message);
typedef void (*LogCloseFn)(void* user_data);
typedef void (*LogFlushFn)(void* user_data);

#ifdef __cplusplus
extern "C"
{
#endif
    // Initialization/Exit functions are thread unsafe
    // appName   used to create appName.log. Pass NULL to disable
    // level     mask of LogLevel bits. Log is ignored if its level is missing in mask. Use All to enable full log
    FORGE_API void InitLog(const char* appName, LogLevel level);
    FORGE_API void ExitLog(void);

    FORGE_API void AddLogFile(const char* filename, FileMode file_mode, LogLevel log_level);
    FORGE_API void AddLogCallback(const char* id, uint32_t log_level, void* user_data, LogCallbackFn callback, LogCloseFn close,
                                  LogFlushFn flush);

    FORGE_API void WriteLogVaList(uint32_t level, const char* filename, int line_number, const char* message, va_list args);
    //+V576, function:WriteLog, format_arg:4, ellipsis_arg:5
    FORGE_API void WriteLog(uint32_t level, const char* filename, int line_number, const char* message, ...);
    //+V576, function:WriteRawLog, format_arg:3, ellipsis_arg:4
    FORGE_API void WriteRawLog(uint32_t level, bool error, const char* message, ...);

    //+V576, function:FailedAssert, format_arg:4, ellipsis_arg:5
    FORGE_API void FailedAssert(const char* file, int line, const char* statement, const char* msg, ...);

    // Usage:
    // puts(HumanReadableTime(ns).str);
    // printf("%s\n", HumanReadableTime(ms * 1000).str);
    // HumanReadableSize(9238412498).str;
    struct HumanReadableValue
    {
        char str[16];
    };
    FORGE_API struct HumanReadableValue HumanReadableSSize(ssize_t Size);
    FORGE_API struct HumanReadableValue HumanReadableTimeD(double nanoseconds);

    static inline struct HumanReadableValue HumanReadableSize(size_t Size) { return HumanReadableSSize((ssize_t)Size); }
    static inline struct HumanReadableValue HumanReadableTime(int64_t nanoseconds) { return HumanReadableTimeD((double)nanoseconds); }

#ifdef __cplusplus
} // extern "C"
#endif
