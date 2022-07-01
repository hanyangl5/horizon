#pragma once

#include "../Definations.h"

#ifdef RENDERDOC_ENABLED

#include "renderdoc_app.h"
#include <libloaderapi.h>
#include <cassert>

namespace Horizon::RDC {

	static RENDERDOC_API_1_5_0* rdoc_api = NULL;
	static bool is_rdc_initialized = false;
	void InitializeRenderDoc() noexcept;
	void StartFrameCapture() noexcept;
	void EndFrameCapture() noexcept;


}
#endif // RENDERDOC_ENABLED


