/*****************************************************************/ /**
 * \file   Config.h
 * \brief  
 * 
 * \author hylu
 * \date   November 2022
 *********************************************************************/
#pragma once

#include <filesystem>

#include <runtime/core/utils/definations.h>
#include <runtime/rhi/Enums.h>

namespace Horizon {

enum class ApplicationType { GRAPHICS, OFFSCREEN_GRAPHICS, GENERAL_COMPUTE };

struct Config {
    u32 width, height;
    RenderBackend render_backend;
    ApplicationType app_type;
    Window *window;
};

} // namespace Horizon