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

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

    typedef bool (*LessFn)(const void* pLhs, const void* pRhs, void* pUserData);

    /*
     * Generic C algorithms
     * 2-8 times slower than non-generic functions.
     * Use macros from AlgorithmsImpl to create non-generic functions for performance critical code
     */

    void Sort(void* pData, size_t memberCount, size_t memberSize, LessFn less, void* pUserData);

    void StableSort(void* pData, size_t memberCount, size_t memberSize, LessFn less, void* pUserData);

    size_t Partition(void* pData, size_t pivot, size_t memberCount, size_t memberSize, LessFn less, void* pUserData);

    /*
     * Numeric C algorithms
     */

    void SortInt8(int8_t* pData, size_t memberCount);
    void SortInt16(int16_t* pData, size_t memberCount);
    void SortInt32(int32_t* pData, size_t memberCount);
    void SortInt64(int64_t* pData, size_t memberCount);

    void SortUInt8(uint8_t* pData, size_t memberCount);
    void SortUInt16(uint16_t* pData, size_t memberCount);
    void SortUInt32(uint32_t* pData, size_t memberCount);
    void SortUInt64(uint64_t* pData, size_t memberCount);

    void SortFloat(float* pData, size_t memberCount);
    void SortDouble(double* pData, size_t memberCount);

    void StableSortInt8(int8_t* pData, size_t memberCount);
    void StableSortInt16(int16_t* pData, size_t memberCount);
    void StableSortInt32(int32_t* pData, size_t memberCount);
    void StableSortInt64(int64_t* pData, size_t memberCount);

    void StableSortUInt8(uint8_t* pData, size_t memberCount);
    void StableSortUInt16(uint16_t* pData, size_t memberCount);
    void StableSortUInt32(uint32_t* pData, size_t memberCount);
    void StableSortUInt64(uint64_t* pData, size_t memberCount);

    void StableSortFloat(float* pData, size_t memberCount);
    void StableSortDouble(double* pData, size_t memberCount);

    size_t PartitionInt8(int8_t* pData, size_t pivot, size_t memberCount);
    size_t PartitionInt16(int16_t* pData, size_t pivot, size_t memberCount);
    size_t PartitionInt32(int32_t* pData, size_t pivot, size_t memberCount);
    size_t PartitionInt64(int64_t* pData, size_t pivot, size_t memberCount);

    size_t PartitionUInt8(uint8_t* pData, size_t pivot, size_t memberCount);
    size_t PartitionUInt16(uint16_t* pData, size_t pivot, size_t memberCount);
    size_t PartitionUInt32(uint32_t* pData, size_t pivot, size_t memberCount);
    size_t PartitionUInt64(uint64_t* pData, size_t pivot, size_t memberCount);

    size_t PartitionFloat(float* pData, size_t pivot, size_t memberCount);
    size_t PartitionDouble(double* pData, size_t pivot, size_t memberCount);

#ifdef __cplusplus
}
#endif // __cplusplus
