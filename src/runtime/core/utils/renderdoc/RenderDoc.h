/*****************************************************************/ /**
 * \file   RenderDoc.h
 * \brief  
 * 
 * \author hylu
 * \date   November 2022
 *********************************************************************/

#pragma once

#include <cassert>

#include "../definations.h"

#ifdef RENDERDOC_ENABLED

#include "renderdoc_app.h"
#include <libloaderapi.h>

namespace Horizon::RDC {

static RENDERDOC_API_1_5_0 *rdoc_api = nullptr;

static bool is_rdc_initialized = false;

void InitializeRenderDoc() noexcept;

void StartFrameCapture() noexcept;

void EndFrameCapture() noexcept;

} // namespace Horizon::RDC

#endif // RENDERDOC_ENABLED
