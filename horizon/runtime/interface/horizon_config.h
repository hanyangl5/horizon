/*****************************************************************//**
 * \file   InitilizationConfig.h
 * \brief  
 * 
 * \author hylu
 * \date   November 2022
 *********************************************************************/

#include <filesystem>

#include <runtime/core/utils/definations.h>
#include <runtime/function/rhi/rhi_utils.h>

namespace Horizon {

enum class ApplicationType {
    GRAPHICS,
    OFFSCREEN_GRAPHICS,
    GENERAL_COMPUTE
};

struct InitilizationConfig {
    u32 width, height;
    RenderBackend render_backend;
    Container::String asset_path;
    bool offscreen;
    ApplicationType app_type;
};

} // namespace Horizon