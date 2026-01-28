#pragma once

#include <chrono>
#include <filesystem>
#include <random>

#include <runtime/core/log/log.h>
#include <runtime/core/math/math.h>
#include <runtime/core/utils/definations.h>
#include <runtime/core/utils/renderdoc/renderdoc.h>
#include <runtime/core/window/window.h>

#include <runtime/render/render.h>
#include <runtime/resource/resource_loader/mesh/meshloader.h>
#include <runtime/resource/resources/mesh/mesh.h>
#include <runtime/rhi/enums.h>
#include <runtime/rhi/rhi.h>
#include <runtime/scene/light/light.h>

// TODO(hyl5) : move to source file
using namespace Horizon;
using namespace Horizon::Backend;

static constexpr u32 width = 1600, height = 900;

extern std::filesystem::path shader_dir;
extern std::filesystem::path asset_path;
