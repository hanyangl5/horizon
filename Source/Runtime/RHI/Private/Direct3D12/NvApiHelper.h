#pragma once

#if defined(_WINDOWS) && !defined(DURANGO)
#include <ThirdParty/nvapi/nvapi.h>
#define NVAPI
#else
typedef enum NvAPI_Status
{
    NVAPI_OK = 0,
    NVAPI_ERROR = -1,
} NvAPI_Status;
#endif

#if defined(NVAPI)
typedef struct
{
    NvU32             driverVersion;
    NvAPI_ShortString buildBranch;
} NvGPUInfo;
static NvAPI_Status gNvStatus = NVAPI_ERROR;
static NvGPUInfo    gNvGpuInfo = {};
#endif

static NvAPI_Status nvapiInit()
{
#if defined(NVAPI)
    gNvStatus = NvAPI_Initialize();
    return gNvStatus;
#endif

    return NvAPI_Status::NVAPI_OK;
}

static void nvapiExit()
{
#if defined(NVAPI)
    NvAPI_Unload();
#endif
}

static void nvapiPrintDriverInfo()
{
#if defined(NVAPI)
    NvAPI_Status status = NvAPI_SYS_GetDriverAndBranchVersion(&gNvGpuInfo.driverVersion, gNvGpuInfo.buildBranch);
    if (NvAPI_Status::NVAPI_OK == status)
    {
        LOGF(eINFO, "NVIDIA Display Driver Version %u", gNvGpuInfo.driverVersion);
        LOGF(eINFO, "NVIDIA Build Branch %s", gNvGpuInfo.buildBranch);
    }
#endif
}
