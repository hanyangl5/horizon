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

#include "Core/ITime.h"
#include "Core/ITime.h"

void InitTimer(Timer* Timer) { ResetTimer(Timer); }

unsigned GetTimerMSec(Timer* Timer, bool Reset)
{
    unsigned CurrentTime = GetTickCountMs();
    unsigned ElapsedTime = CurrentTime - Timer->StartTime;
    if (Reset)
    {
        Timer->StartTime = CurrentTime;
    }

    return ElapsedTime;
}

void ResetTimer(Timer* Timer) { Timer->StartTime = GetTickCountMs(); }

void InitHiresTimer(HiresTimer* Timer)
{
    *Timer = (HiresTimer){ 0 };
    ResetHiresTimer(Timer);
}

int64_t GetHiresTimerUSec(HiresTimer* Timer, bool Reset)
{
    int64_t CurrentTime = GetUSec(true);
    int64_t ElapsedTime = CurrentTime - Timer->StartTime;

    // Correct for possible weirdness with changing internal frequency
    if (ElapsedTime < 0)
    {
        ElapsedTime = 0;
    }

    if (Reset)
    {
        Timer->StartTime = CurrentTime;
    }

    Timer->History[Timer->HistoryIndex] = ElapsedTime;
    Timer->HistoryIndex = (Timer->HistoryIndex + 1) % HIRES_TIMER_LENGTH_OF_HISTORY;

    return ElapsedTime;
}

int64_t GetHiresTimerUSecAverage(HiresTimer* Timer)
{
    int64_t ElapsedTime = 0;
    for (uint32_t i = 0; i < HIRES_TIMER_LENGTH_OF_HISTORY; ++i)
        ElapsedTime += Timer->History[i];
    ElapsedTime /= HIRES_TIMER_LENGTH_OF_HISTORY;

    // Correct for overflow
    if (ElapsedTime < 0)
    {
        ElapsedTime = 0;
    }

    return ElapsedTime;
}

float GetHiresTimerSeconds(HiresTimer* Timer, bool Reset) { return (float)(GetHiresTimerUSec(Timer, Reset) / 1e6); }

float GetHiresTimerSecondsAverage(HiresTimer* Timer) { return (float)(GetHiresTimerUSecAverage(Timer) / 1e6); }

void ResetHiresTimer(HiresTimer* Timer) { Timer->StartTime = GetUSec(true); }