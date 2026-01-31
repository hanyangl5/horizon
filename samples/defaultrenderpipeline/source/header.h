#pragma once

#include <chrono>
#include <filesystem>
#include <random>

#include <core/log.h>
#include <core/math.h>
#include <core/definations.h>
#include <core/renderdoc/render_doc.h>
#include <core/glfwwindow.h>

#include <resource/resource_loader/mesh/mesh_loader.h>
#include <resource/resources/mesh/mesh.h>
#include <scene/scene_renderer/renderer.h>

#include <rhi/enums.h>
#include <rhi/rhi.h>
#include <scene/light/light.h>

// TODO(hyl5) : move to source file
using namespace Horizon;
using namespace Horizon::Backend;

static constexpr u32 width = 1600, height = 900;

extern std::filesystem::path shader_dir;
extern std::filesystem::path asset_path;
