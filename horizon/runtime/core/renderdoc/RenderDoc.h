/*****************************************************************//**
 * \file   RenderDoc.h
 * \brief  
 * 
 * \author hylu
 * \date   November 2022
 *********************************************************************/

#pragma once

#ifdef RENDERDOC_ENABLED

// standard libraries
#include <cassert>

// third party libraries
#include <libloaderapi.h>
#include "renderdoc_app.h"

// project headers
#include <runtime/core/utils/definations.h>

namespace Horizon::RDC {

static RENDERDOC_API_1_5_0 *rdoc_api = nullptr;

static bool is_rdc_initialized = false;

void InitializeRenderDoc() noexcept;

void StartFrameCapture() noexcept;

void EndFrameCapture() noexcept;

} // namespace Horizon::RDC

#endif // RENDERDOC_ENABLED
