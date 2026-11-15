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

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* prevent 64-bit overflow when computing relative timestamp
        see https://gist.github.com/jspohr/3dc4f00033d79ec5bdaf67bc46c813e3
    */
    /* TODO: Move this function to the Math Library as soon as it's in C */
    static inline int64_t Int64MulDiv(int64_t Value, int64_t Numer, int64_t Denom)
    {
        int64_t q = Value / Denom;
        int64_t r = Value % Denom;
        return q * Numer + r * Numer / Denom;
    }

    // High res timer functions
    FORGE_API int64_t GetUSec(bool Precise);
    FORGE_API int64_t GetTimerFrequency(void);

    // Time related functions
    FORGE_API uint32_t GetTickCountMs(void);
    FORGE_API uint32_t GetUnixTimeSeconds(void);

    /// Low res OS timer
    typedef struct Timer
    {
        uint32_t StartTime;
    } Timer;

    FORGE_API void     InitTimer(Timer* Timer);
    FORGE_API void     ResetTimer(Timer* Timer);
    FORGE_API uint32_t GetTimerMSec(Timer* Timer, bool Reset);

/// High-resolution OS timer
#define HIRES_TIMER_LENGTH_OF_HISTORY 60

    typedef struct HiresTimer
    {
        int64_t StartTime;

        int64_t  History[HIRES_TIMER_LENGTH_OF_HISTORY];
        uint32_t HistoryIndex;
    } HiresTimer;

    FORGE_API void    InitHiresTimer(HiresTimer* Timer);
    FORGE_API int64_t GetHiresTimerUSec(HiresTimer* Timer, bool Reset);
    FORGE_API int64_t GetHiresTimerUSecAverage(HiresTimer* Timer);
    FORGE_API float   GetHiresTimerSeconds(HiresTimer* Timer, bool Reset);
    FORGE_API float   GetHiresTimerSecondsAverage(HiresTimer* Timer);
    FORGE_API void    ResetHiresTimer(HiresTimer* Timer);

#ifdef __cplusplus
}
#endif
