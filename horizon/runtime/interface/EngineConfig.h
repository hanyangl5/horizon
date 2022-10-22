#include <filesystem>
#include <runtime/core/utils/definations.h>
#include <string>

namespace Horizon {

enum class ApplicationType {
    GRAPHICS,
    OFFSCREEN_GRAPHICS,
    GENERAL_COMPUTE
};

struct EngineConfig {
    u32 width, height;
    RenderBackend render_backend;
    std::string asset_path;
    bool offscreen;
    ApplicationType app_type;
};
} // namespace Horizon