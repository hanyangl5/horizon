#pragma once

#include <chrono>
#include <random>

#include <core/path.h>

#include <core/log.h>
#include <core/math.h>
#include <core/renderdoc/render_doc.h>

#include <resource/resource_loader/mesh/mesh_loader.h>
#include <resource/resources/mesh/mesh.h>
#include <scene/scene_renderer/renderer.h>

#include <rhi/enums.h>
#include <rhi/rhi.h>
#include <scene/light/light.h>

// TODO(hyl5) : move to source file
using namespace Horizon;
using namespace Horizon::Backend;

extern u32 width, height;
extern Horizon::Path shader_dir;
extern Horizon::Path asset_path;

