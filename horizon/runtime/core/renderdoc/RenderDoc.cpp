/*****************************************************************//**
 * \file   RenderDoc.cpp
 * \brief  
 * 
 * \author hylu
 * \date   November 2022
 *********************************************************************/

#include "renderdoc.h"

#include <runtime/core/log/log.h>

#ifdef RENDERDOC_ENABLED

void Horizon::RDC::InitializeRenderDoc() noexcept {

    is_rdc_initialized = true;
    // At init, on windows
    if (HMODULE mod = GetModuleHandleA("renderdoc.dll")) {
        auto RENDERDOC_GetAPI{(pRENDERDOC_GetAPI)GetProcAddress(mod, "RENDERDOC_GetAPI")};
        int ret{RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_5_0, (void **)&rdoc_api)};
        assert(ret == 1);
    } else {
        LOG_ERROR("failed to find renderdoc.dll");
        return;
    }

    // At init, on linux/android.
    // For android replace librenderdoc.so with libVkLayer_GLES_RenderDoc.so
    // if (void* mod = dlopen("librenderdoc.so", RTLD_NOW | RTLD_NOLOAD))
    //{
    //    pRENDERDOC_GetAPI RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)dlsym(mod,
    //    "RENDERDOC_GetAPI"); int ret =
    //    RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_5_0, (void**)&rdoc_api);
    //    assert(ret == 1);
    //}
    LOG_INFO("renderdoc api initialized");
}

// To start a frame capture, call StartFrameCapture.
// You can specify NULL, NULL for the device to capture on if you have only one
// device and either no windows at all or only one window, and it will capture
// from that device. See the documentation below for a longer explanation

void Horizon::RDC::StartFrameCapture() noexcept {
    if (!is_rdc_initialized) {
        InitializeRenderDoc();
    }
    // Your rendering should happen here
    if (rdoc_api) {
        rdoc_api->StartFrameCapture(nullptr, nullptr);
        LOG_DEBUG("frame capture start");
    }
}

void Horizon::RDC::EndFrameCapture() noexcept {
    // stop the capture
    if (rdoc_api) {
        rdoc_api->EndFrameCapture(nullptr, nullptr);
        LOG_DEBUG("frame capture end");
    }
}

#endif