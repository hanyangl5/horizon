#include <filesystem>
#include <runtime/core/utils/definations.h>
#include <string>

namespace Horizon {

struct EngineConfig {
    u32 width, height;
    RenderBackend render_backend;
    std::string asset_path;
    bool offscreen;
};
} // namespace Horizon