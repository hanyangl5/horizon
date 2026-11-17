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

#include "Algorithms.h"

#include "Core/ILog.h"

#include "AlgorithmsImpl.h"

#include "Core/IMemory.h"

// SIMPLE SORT
// Used for array sizes 0..5

static void simpleSort(void* pVoidData, size_t memberCount, size_t memberSize, LessFn less, void* UserData)
{
#define SIMPLE_SORT_FN(pArr, Count) simpleSort(pArr, Count, memberSize, less, UserData)
    SIMPLE_SORT_IMPL(SIMPLE_SORT_FN, char, (char*)pVoidData, memberCount, LESS_GENERIC, CREATE_TEMP_GENERIC, DESTROY_TEMP_GENERIC,
                     COPY_GENERIC, PTR_INC_GENERIC, PTR_ADD_GENERIC, PTR_SUB_GENERIC)
#undef SIMPLE_SORT_FN
}

static void simpleSortInt8(int8_t* pArr, size_t memberCount)
{
    SIMPLE_SORT_IMPL(simpleSortInt8, int8_t, pArr, memberCount, LESS_NUMERIC, CREATE_TEMP_NUMERIC, DESTROY_TEMP_NUMERIC, COPY_NUMERIC,
                     PTR_INC_NUMERIC, PTR_ADD_NUMERIC, PTR_SUB_NUMERIC)
}
static void simpleSortInt16(int16_t* pArr, size_t memberCount)
{
    SIMPLE_SORT_IMPL(simpleSortInt16, int16_t, pArr, memberCount, LESS_NUMERIC, CREATE_TEMP_NUMERIC, DESTROY_TEMP_NUMERIC, COPY_NUMERIC,
                     PTR_INC_NUMERIC, PTR_ADD_NUMERIC, PTR_SUB_NUMERIC)
}
static void simpleSortInt32(int32_t* pArr, size_t memberCount)
{
    SIMPLE_SORT_IMPL(simpleSortInt32, int32_t, pArr, memberCount, LESS_NUMERIC, CREATE_TEMP_NUMERIC, DESTROY_TEMP_NUMERIC, COPY_NUMERIC,
                     PTR_INC_NUMERIC, PTR_ADD_NUMERIC, PTR_SUB_NUMERIC)
}
static void simpleSortInt64(int64_t* pArr, size_t memberCount)
{
    SIMPLE_SORT_IMPL(simpleSortInt64, int64_t, pArr, memberCount, LESS_NUMERIC, CREATE_TEMP_NUMERIC, DESTROY_TEMP_NUMERIC, COPY_NUMERIC,
                     PTR_INC_NUMERIC, PTR_ADD_NUMERIC, PTR_SUB_NUMERIC)
}

static void simpleSortUInt8(uint8_t* pArr, size_t memberCount)
{
    SIMPLE_SORT_IMPL(simpleSortUInt8, uint8_t, pArr, memberCount, LESS_NUMERIC, CREATE_TEMP_NUMERIC, DESTROY_TEMP_NUMERIC, COPY_NUMERIC,
                     PTR_INC_NUMERIC, PTR_ADD_NUMERIC, PTR_SUB_NUMERIC)
}
static void simpleSortUInt16(uint16_t* pArr, size_t memberCount)
{
    SIMPLE_SORT_IMPL(simpleSortUInt16, uint16_t, pArr, memberCount, LESS_NUMERIC, CREATE_TEMP_NUMERIC, DESTROY_TEMP_NUMERIC, COPY_NUMERIC,
                     PTR_INC_NUMERIC, PTR_ADD_NUMERIC, PTR_SUB_NUMERIC)
}
static void simpleSortUInt32(uint32_t* pArr, size_t memberCount)
{
    SIMPLE_SORT_IMPL(simpleSortUInt32, uint32_t, pArr, memberCount, LESS_NUMERIC, CREATE_TEMP_NUMERIC, DESTROY_TEMP_NUMERIC, COPY_NUMERIC,
                     PTR_INC_NUMERIC, PTR_ADD_NUMERIC, PTR_SUB_NUMERIC)
}
static void simpleSortUInt64(uint64_t* pArr, size_t memberCount)
{
    SIMPLE_SORT_IMPL(simpleSortUInt64, uint64_t, pArr, memberCount, LESS_NUMERIC, CREATE_TEMP_NUMERIC, DESTROY_TEMP_NUMERIC, COPY_NUMERIC,
                     PTR_INC_NUMERIC, PTR_ADD_NUMERIC, PTR_SUB_NUMERIC)
}

static void simpleSortFloat(float* pArr, size_t memberCount)
{
    SIMPLE_SORT_IMPL(simpleSortFloat, float, pArr, memberCount, LESS_NUMERIC, CREATE_TEMP_NUMERIC, DESTROY_TEMP_NUMERIC, COPY_NUMERIC,
                     PTR_INC_NUMERIC, PTR_ADD_NUMERIC, PTR_SUB_NUMERIC)
}
static void simpleSortDouble(double* pArr, size_t memberCount)
{
    SIMPLE_SORT_IMPL(simpleSortDouble, double, pArr, memberCount, LESS_NUMERIC, CREATE_TEMP_NUMERIC, DESTROY_TEMP_NUMERIC, COPY_NUMERIC,
                     PTR_INC_NUMERIC, PTR_ADD_NUMERIC, PTR_SUB_NUMERIC)
}

// INSERTION SORT (stable Sort)

void StableSort(void* pVoidData, size_t memberCount, size_t memberSize, LessFn less, void* UserData)
{
#define SIMPLE_SORT(pArr, memberCount) simpleSort(pArr, memberCount, memberSize, less, UserData)
    INSERTION_SORT_IMPL(char, (char*)pVoidData, memberCount, SIMPLE_SORT, LESS_GENERIC, CREATE_TEMP_GENERIC, DESTROY_TEMP_GENERIC,
                        COPY_GENERIC, PTR_INC_GENERIC, PTR_DEC_GENERIC, PTR_ADD_GENERIC, PTR_SUB_GENERIC)
#undef SIMPLE_SORT
}

void StableSortInt8(int8_t* pArr, size_t memberCount)
{
    INSERTION_SORT_IMPL(int8_t, pArr, memberCount, simpleSortInt8, LESS_NUMERIC, CREATE_TEMP_NUMERIC, DESTROY_TEMP_NUMERIC, COPY_NUMERIC,
                        PTR_INC_NUMERIC, PTR_DEC_NUMERIC, PTR_ADD_NUMERIC, PTR_SUB_NUMERIC)
}
void StableSortInt16(int16_t* pArr, size_t memberCount)
{
    INSERTION_SORT_IMPL(int16_t, pArr, memberCount, simpleSortInt16, LESS_NUMERIC, CREATE_TEMP_NUMERIC, DESTROY_TEMP_NUMERIC, COPY_NUMERIC,
                        PTR_INC_NUMERIC, PTR_DEC_NUMERIC, PTR_ADD_NUMERIC, PTR_SUB_NUMERIC)
}
void StableSortInt32(int32_t* pArr, size_t memberCount)
{
    INSERTION_SORT_IMPL(int32_t, pArr, memberCount, simpleSortInt32, LESS_NUMERIC, CREATE_TEMP_NUMERIC, DESTROY_TEMP_NUMERIC, COPY_NUMERIC,
                        PTR_INC_NUMERIC, PTR_DEC_NUMERIC, PTR_ADD_NUMERIC, PTR_SUB_NUMERIC)
}
void StableSortInt64(int64_t* pArr, size_t memberCount)
{
    INSERTION_SORT_IMPL(int64_t, pArr, memberCount, simpleSortInt64, LESS_NUMERIC, CREATE_TEMP_NUMERIC, DESTROY_TEMP_NUMERIC, COPY_NUMERIC,
                        PTR_INC_NUMERIC, PTR_DEC_NUMERIC, PTR_ADD_NUMERIC, PTR_SUB_NUMERIC)
}

void StableSortUInt8(uint8_t* pArr, size_t memberCount)
{
    INSERTION_SORT_IMPL(uint8_t, pArr, memberCount, simpleSortUInt8, LESS_NUMERIC, CREATE_TEMP_NUMERIC, DESTROY_TEMP_NUMERIC, COPY_NUMERIC,
                        PTR_INC_NUMERIC, PTR_DEC_NUMERIC, PTR_ADD_NUMERIC, PTR_SUB_NUMERIC)
}
void StableSortUInt16(uint16_t* pArr, size_t memberCount)
{
    INSERTION_SORT_IMPL(uint16_t, pArr, memberCount, simpleSortUInt16, LESS_NUMERIC, CREATE_TEMP_NUMERIC, DESTROY_TEMP_NUMERIC,
                        COPY_NUMERIC, PTR_INC_NUMERIC, PTR_DEC_NUMERIC, PTR_ADD_NUMERIC, PTR_SUB_NUMERIC)
}
void StableSortUInt32(uint32_t* pArr, size_t memberCount)
{
    INSERTION_SORT_IMPL(uint32_t, pArr, memberCount, simpleSortUInt32, LESS_NUMERIC, CREATE_TEMP_NUMERIC, DESTROY_TEMP_NUMERIC,
                        COPY_NUMERIC, PTR_INC_NUMERIC, PTR_DEC_NUMERIC, PTR_ADD_NUMERIC, PTR_SUB_NUMERIC)
}
void StableSortUInt64(uint64_t* pArr, size_t memberCount)
{
    INSERTION_SORT_IMPL(uint64_t, pArr, memberCount, simpleSortUInt64, LESS_NUMERIC, CREATE_TEMP_NUMERIC, DESTROY_TEMP_NUMERIC,
                        COPY_NUMERIC, PTR_INC_NUMERIC, PTR_DEC_NUMERIC, PTR_ADD_NUMERIC, PTR_SUB_NUMERIC)
}

void StableSortFloat(float* pArr, size_t memberCount)
{
    INSERTION_SORT_IMPL(float, pArr, memberCount, simpleSortFloat, LESS_NUMERIC, CREATE_TEMP_NUMERIC, DESTROY_TEMP_NUMERIC, COPY_NUMERIC,
                        PTR_INC_NUMERIC, PTR_DEC_NUMERIC, PTR_ADD_NUMERIC, PTR_SUB_NUMERIC)
}
void StableSortDouble(double* pArr, size_t memberCount)
{
    INSERTION_SORT_IMPL(double, pArr, memberCount, simpleSortDouble, LESS_NUMERIC, CREATE_TEMP_NUMERIC, DESTROY_TEMP_NUMERIC, COPY_NUMERIC,
                        PTR_INC_NUMERIC, PTR_DEC_NUMERIC, PTR_ADD_NUMERIC, PTR_SUB_NUMERIC)
}

// PARTITION
// V_RET_NOT_NULL, function:partitionImpl
static char* partitionImpl(char* pBegin, char* pEnd, char* pPivot, size_t memberSize, LessFn less,
                           void* UserData){ PARTITION_IMPL(char, pBegin, pEnd, pPivot, LESS_GENERIC, CREATE_TEMP_GENERIC,
                                                           DESTROY_TEMP_GENERIC, COPY_GENERIC, PTR_INC_GENERIC, PTR_SUB_GENERIC) } size_t
    Partition(void* Data, size_t pivot, size_t memberCount, size_t memberSize, LessFn less, void* UserData)
{
    if (memberCount == 0)
        return 0;
    char* pPivot =
        partitionImpl((char*)Data, (char*)Data + memberSize * memberCount, (char*)Data + pivot * memberSize, memberSize, less, UserData);
    return (pPivot - (char*)Data) / memberSize;
}

// V_RET_NOT_NULL, function:partitionImplInt8
static int8_t* partitionImplInt8(int8_t* pBegin, int8_t* pEnd, int8_t* pPivot){
    PARTITION_IMPL(int8_t, pBegin, pEnd, pPivot, LESS_NUMERIC, CREATE_TEMP_NUMERIC, DESTROY_TEMP_NUMERIC, COPY_NUMERIC, PTR_INC_NUMERIC,
                   PTR_SUB_NUMERIC)
} size_t PartitionInt8(int8_t* pArr, size_t pivot, size_t memberCount)
{
    if (memberCount == 0)
        return 0;
    int8_t* pPivot = partitionImplInt8(pArr, pArr + memberCount, pArr + pivot);
    return pPivot - pArr;
}

// V_RET_NOT_NULL, function:partitionImplInt16
static int16_t* partitionImplInt16(int16_t* pBegin, int16_t* pEnd, int16_t* pPivot){
    PARTITION_IMPL(int16_t, pBegin, pEnd, pPivot, LESS_NUMERIC, CREATE_TEMP_NUMERIC, DESTROY_TEMP_NUMERIC, COPY_NUMERIC, PTR_INC_NUMERIC,
                   PTR_SUB_NUMERIC)
} size_t PartitionInt16(int16_t* pArr, size_t pivot, size_t memberCount)
{
    if (memberCount == 0)
        return 0;
    int16_t* pPivot = partitionImplInt16(pArr, pArr + memberCount, pArr + pivot);
    return pPivot - pArr;
}
// V_RET_NOT_NULL, function:partitionImplInt32
static int32_t* partitionImplInt32(int32_t* pBegin, int32_t* pEnd, int32_t* pPivot){
    PARTITION_IMPL(int32_t, pBegin, pEnd, pPivot, LESS_NUMERIC, CREATE_TEMP_NUMERIC, DESTROY_TEMP_NUMERIC, COPY_NUMERIC, PTR_INC_NUMERIC,
                   PTR_SUB_NUMERIC)
} size_t PartitionInt32(int32_t* pArr, size_t pivot, size_t memberCount)
{
    if (memberCount == 0)
        return 0;
    int32_t* pPivot = partitionImplInt32(pArr, pArr + memberCount, pArr + pivot);
    return pPivot - pArr;
}
// V_RET_NOT_NULL, function:partitionImplInt64
static int64_t* partitionImplInt64(int64_t* pBegin, int64_t* pEnd, int64_t* pPivot){
    PARTITION_IMPL(int64_t, pBegin, pEnd, pPivot, LESS_NUMERIC, CREATE_TEMP_NUMERIC, DESTROY_TEMP_NUMERIC, COPY_NUMERIC, PTR_INC_NUMERIC,
                   PTR_SUB_NUMERIC)
} size_t PartitionInt64(int64_t* pArr, size_t pivot, size_t memberCount)
{
    if (memberCount == 0)
        return 0;
    int64_t* pPivot = partitionImplInt64(pArr, pArr + memberCount, pArr + pivot);
    return pPivot - pArr;
}

// V_RET_NOT_NULL, function:partitionImplUInt8
static uint8_t* partitionImplUInt8(uint8_t* pBegin, uint8_t* pEnd,
                                   uint8_t* pPivot){ PARTITION_IMPL(uint8_t, pBegin, pEnd, pPivot, LESS_NUMERIC, CREATE_TEMP_NUMERIC,
                                                                    DESTROY_TEMP_NUMERIC, COPY_NUMERIC, PTR_INC_NUMERIC, PTR_SUB_NUMERIC) }
// V_RET_NOT_NULL, function:PartitionUInt8
size_t PartitionUInt8(uint8_t* pArr, size_t pivot, size_t memberCount)
{
    if (memberCount == 0)
        return 0;
    uint8_t* pPivot = partitionImplUInt8(pArr, pArr + memberCount, pArr + pivot);
    return pPivot - pArr;
}
// V_RET_NOT_NULL, function:partitionImplUInt16
static uint16_t* partitionImplUInt16(uint16_t* pBegin, uint16_t* pEnd, uint16_t* pPivot){
    PARTITION_IMPL(uint16_t, pBegin, pEnd, pPivot, LESS_NUMERIC, CREATE_TEMP_NUMERIC, DESTROY_TEMP_NUMERIC, COPY_NUMERIC, PTR_INC_NUMERIC,
                   PTR_SUB_NUMERIC)
} // V_RET_NOT_NULL, function:PartitionUInt16
size_t PartitionUInt16(uint16_t* pArr, size_t pivot, size_t memberCount)
{
    if (memberCount == 0)
        return 0;
    uint16_t* pPivot = partitionImplUInt16(pArr, pArr + memberCount, pArr + pivot);
    return pPivot - pArr;
}
// V_RET_NOT_NULL, function:partitionImplUInt32
static uint32_t* partitionImplUInt32(uint32_t* pBegin, uint32_t* pEnd, uint32_t* pPivot){
    PARTITION_IMPL(uint32_t, pBegin, pEnd, pPivot, LESS_NUMERIC, CREATE_TEMP_NUMERIC, DESTROY_TEMP_NUMERIC, COPY_NUMERIC, PTR_INC_NUMERIC,
                   PTR_SUB_NUMERIC)
} size_t PartitionUInt32(uint32_t* pArr, size_t pivot, size_t memberCount)
{
    if (memberCount == 0)
        return 0;
    uint32_t* pPivot = partitionImplUInt32(pArr, pArr + memberCount, pArr + pivot);
    return pPivot - pArr;
}
// V_RET_NOT_NULL, function:partitionImplUInt64
static uint64_t* partitionImplUInt64(uint64_t* pBegin, uint64_t* pEnd, uint64_t* pPivot){
    PARTITION_IMPL(uint64_t, pBegin, pEnd, pPivot, LESS_NUMERIC, CREATE_TEMP_NUMERIC, DESTROY_TEMP_NUMERIC, COPY_NUMERIC, PTR_INC_NUMERIC,
                   PTR_SUB_NUMERIC)
} // V_RET_NOT_NULL, function:PartitionUInt64
size_t PartitionUInt64(uint64_t* pArr, size_t pivot, size_t memberCount)
{
    if (memberCount == 0)
        return 0;
    uint64_t* pPivot = partitionImplUInt64(pArr, pArr + memberCount, pArr + pivot);
    return pPivot - pArr;
}

// V_RET_NOT_NULL, function:partitionImplFloat
static float* partitionImplFloat(float* pBegin, float* pEnd, float* pPivot){
    PARTITION_IMPL(float, pBegin, pEnd, pPivot, LESS_NUMERIC, CREATE_TEMP_NUMERIC, DESTROY_TEMP_NUMERIC, COPY_NUMERIC, PTR_INC_NUMERIC,
                   PTR_SUB_NUMERIC)
} size_t PartitionFloat(float* pArr, size_t pivot, size_t memberCount)
{
    if (memberCount == 0)
        return 0;
    float* pPivot = partitionImplFloat(pArr, pArr + memberCount, pArr + pivot);
    return pPivot - pArr;
}
// V_RET_NOT_NULL, function:partitionImplDouble
static double* partitionImplDouble(double* pBegin, double* pEnd, double* pPivot){
    PARTITION_IMPL(double, pBegin, pEnd, pPivot, LESS_NUMERIC, CREATE_TEMP_NUMERIC, DESTROY_TEMP_NUMERIC, COPY_NUMERIC, PTR_INC_NUMERIC,
                   PTR_SUB_NUMERIC)
} size_t PartitionDouble(double* pArr, size_t pivot, size_t memberCount)
{
    if (memberCount == 0)
        return 0;
    double* pPivot = partitionImplDouble(pArr, pArr + memberCount, pArr + pivot);
    return pPivot - pArr;
}

static void QuickSort(char* pBegin, char* pEnd, char* tmp, size_t memberSize, LessFn less, void* UserData)
{
#define QUICKSORT_IMPL_FN(pBegin, pEnd, tmp)    QuickSort(pBegin, pEnd, tmp, memberSize, less, UserData);
#define FALLBACK_SORT(pArr, memberCount)        StableSort(pArr, memberCount, memberSize, less, UserData)
#define PARTITION_IMPL_FN(pBegin, pEnd, pPivot) partitionImpl(pBegin, pEnd, pPivot, memberSize, less, UserData)

    QUICKSORT_IMPL(char, pBegin, pEnd, tmp, QUICKSORT_IMPL_FN, FALLBACK_SORT, PARTITION_IMPL_FN, LESS_GENERIC, COPY_GENERIC,
                   PTR_ADD_GENERIC, PTR_SUB_GENERIC, PTR_DIFF_GENERIC)
#undef QUICKSORT_IMPL_FN
#undef FALLBACK_SORT
#undef PARTITION_IMPL_FN
}

void Sort(void* Data, size_t memberCount, size_t memberSize, LessFn less, void* UserData)
{
    CREATE_TEMP_GENERIC(char, tmp);
    QuickSort((char*)Data, (char*)Data + memberCount * memberSize, tmp, memberSize, less, UserData);
    DESTROY_TEMP_GENERIC(tmp);
}

static void quickSortInt8(int8_t* pBegin, int8_t* pEnd, int8_t* tmp)
{
    QUICKSORT_IMPL(int8_t, pBegin, pEnd, tmp, quickSortInt8, StableSortInt8, partitionImplInt8, LESS_NUMERIC, COPY_NUMERIC, PTR_ADD_NUMERIC,
                   PTR_SUB_NUMERIC, PTR_DIFF_NUMERIC)
}
void SortInt8(int8_t* Data, size_t memberCount)
{
    int8_t tmp;
    quickSortInt8(Data, Data + memberCount, &tmp);
}
static void quickSortInt16(int16_t* pBegin, int16_t* pEnd, int16_t* tmp)
{
    QUICKSORT_IMPL(int16_t, pBegin, pEnd, tmp, quickSortInt16, StableSortInt16, partitionImplInt16, LESS_NUMERIC, COPY_NUMERIC,
                   PTR_ADD_NUMERIC, PTR_SUB_NUMERIC, PTR_DIFF_NUMERIC)
}
void SortInt16(int16_t* Data, size_t memberCount)
{
    int16_t tmp;
    quickSortInt16(Data, Data + memberCount, &tmp);
}
static void quickSortInt32(int32_t* pBegin, int32_t* pEnd, int32_t* tmp)
{
    QUICKSORT_IMPL(int32_t, pBegin, pEnd, tmp, quickSortInt32, StableSortInt32, partitionImplInt32, LESS_NUMERIC, COPY_NUMERIC,
                   PTR_ADD_NUMERIC, PTR_SUB_NUMERIC, PTR_DIFF_NUMERIC)
}
void SortInt32(int32_t* Data, size_t memberCount)
{
    int32_t tmp;
    quickSortInt32(Data, Data + memberCount, &tmp);
}
static void quickSortInt64(int64_t* pBegin, int64_t* pEnd, int64_t* tmp)
{
    QUICKSORT_IMPL(int64_t, pBegin, pEnd, tmp, quickSortInt64, StableSortInt64, partitionImplInt64, LESS_NUMERIC, COPY_NUMERIC,
                   PTR_ADD_NUMERIC, PTR_SUB_NUMERIC, PTR_DIFF_NUMERIC)
}
void SortInt64(int64_t* Data, size_t memberCount)
{
    int64_t tmp;
    quickSortInt64(Data, Data + memberCount, &tmp);
}

static void quickSortUInt8(uint8_t* pBegin, uint8_t* pEnd, uint8_t* tmp)
{
    QUICKSORT_IMPL(uint8_t, pBegin, pEnd, tmp, quickSortUInt8, StableSortUInt8, partitionImplUInt8, LESS_NUMERIC, COPY_NUMERIC,
                   PTR_ADD_NUMERIC, PTR_SUB_NUMERIC, PTR_DIFF_NUMERIC)
}
void SortUInt8(uint8_t* Data, size_t memberCount)
{
    uint8_t tmp;
    quickSortUInt8(Data, Data + memberCount, &tmp);
}
static void quickSortUInt16(uint16_t* pBegin, uint16_t* pEnd, uint16_t* tmp)
{
    QUICKSORT_IMPL(uint16_t, pBegin, pEnd, tmp, quickSortUInt16, StableSortUInt16, partitionImplUInt16, LESS_NUMERIC, COPY_NUMERIC,
                   PTR_ADD_NUMERIC, PTR_SUB_NUMERIC, PTR_DIFF_NUMERIC)
}
void SortUInt16(uint16_t* Data, size_t memberCount)
{
    uint16_t tmp;
    quickSortUInt16(Data, Data + memberCount, &tmp);
}
static void quickSortUInt32(uint32_t* pBegin, uint32_t* pEnd, uint32_t* tmp)
{
    QUICKSORT_IMPL(uint32_t, pBegin, pEnd, tmp, quickSortUInt32, StableSortUInt32, partitionImplUInt32, LESS_NUMERIC, COPY_NUMERIC,
                   PTR_ADD_NUMERIC, PTR_SUB_NUMERIC, PTR_DIFF_NUMERIC)
}
void SortUInt32(uint32_t* Data, size_t memberCount)
{
    uint32_t tmp;
    quickSortUInt32(Data, Data + memberCount, &tmp);
}
static void quickSortUInt64(uint64_t* pBegin, uint64_t* pEnd, uint64_t* tmp)
{
    QUICKSORT_IMPL(uint64_t, pBegin, pEnd, tmp, quickSortUInt64, StableSortUInt64, partitionImplUInt64, LESS_NUMERIC, COPY_NUMERIC,
                   PTR_ADD_NUMERIC, PTR_SUB_NUMERIC, PTR_DIFF_NUMERIC)
}
void SortUInt64(uint64_t* Data, size_t memberCount)
{
    uint64_t tmp;
    quickSortUInt64(Data, Data + memberCount, &tmp);
}

static void quickSortFloat(float* pBegin, float* pEnd, float* tmp)
{
    QUICKSORT_IMPL(float, pBegin, pEnd, tmp, quickSortFloat, StableSortFloat, partitionImplFloat, LESS_NUMERIC, COPY_NUMERIC,
                   PTR_ADD_NUMERIC, PTR_SUB_NUMERIC, PTR_DIFF_NUMERIC)
}
void SortFloat(float* Data, size_t memberCount)
{
    float tmp;
    quickSortFloat(Data, Data + memberCount, &tmp);
}
static void quickSortDouble(double* pBegin, double* pEnd, double* tmp)
{
    QUICKSORT_IMPL(double, pBegin, pEnd, tmp, quickSortDouble, StableSortDouble, partitionImplDouble, LESS_NUMERIC, COPY_NUMERIC,
                   PTR_ADD_NUMERIC, PTR_SUB_NUMERIC, PTR_DIFF_NUMERIC)
}
void SortDouble(double* Data, size_t memberCount)
{
    double tmp;
    quickSortDouble(Data, Data + memberCount, &tmp);
}
