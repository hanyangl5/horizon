#pragma once

#if defined(_WINDOWS) && !defined(DURANGO)
#include <ThirdParty/ags/ags_lib/inc/amd_ags.h>
//#define AMDAGS
#else
enum AGSReturnCode
{
    AGS_SUCCESS,
    AGS_FAILURE,
    AGS_INVALID_ARGS,
    AGS_OUT_OF_MEMORY,
    AGS_MISSING_D3D_DLL,
    AGS_LEGACY_DRIVER,
    AGS_NO_AMD_DRIVER_INSTALLED,
    AGS_EXTENSION_NOT_SUPPORTED,
    AGS_ADL_FAILURE,
    AGS_DX_FAILURE
};
#endif

#if defined(AMDAGS)
static AGSReturnCode gAgsStatus = AGS_FAILURE;
static AGSContext*   pAgsContext = NULL;
static AGSGPUInfo    gAgsGpuInfo = {};
#endif

static AGSReturnCode agsInit()
{
#if defined(AMDAGS)
    AGSConfiguration config = {};
    gAgsStatus = agsInit(&pAgsContext, &config, &gAgsGpuInfo);
    return gAgsStatus;
#endif

    return AGS_SUCCESS;
}

static void agsExit()
{
#if defined(AMDAGS)
    agsDeInit(pAgsContext);
#endif
}

static void agsPrintDriverInfo()
{
#if defined(AMDAGS)
    if (pAgsContext)
    {
        LOGF(eINFO, "AMD Display Driver Version %u", gAgsGpuInfo.driverVersion);
        LOGF(eINFO, "AMD Radeon Software Version %s", gAgsGpuInfo.radeonSoftwareVersion);
    }
#endif
}
