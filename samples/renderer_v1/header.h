#pragma once

#include <chrono>
#include <filesystem>
#include <random>

#include <runtime/core/log/Log.h>
#include <runtime/core/math/Math.h>
#include <runtime/core/utils/definations.h>
#include <runtime/core/utils/renderdoc/RenderDoc.h>
#include <runtime/core/window/Window.h>

#include <runtime/render/Render.h>
#include <runtime/resource/resource_loader/mesh/MeshLoader.h>
#include <runtime/resource/resources/mesh/Mesh.h>
#include <runtime/rhi/RHI.h>
#include <runtime/rhi/Enums.h>
#include <runtime/scene/light/Light.h>

// TODO(hyl5) : move to source file
using namespace Horizon;
using namespace Horizon::Backend;

static constexpr u32 width = 1600, height = 900;

extern std::filesystem::path shader_dir;
extern std::filesystem::path asset_path;
