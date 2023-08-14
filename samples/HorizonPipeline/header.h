#pragma once

#include <chrono>
#include <filesystem>
#include <random>

#include <runtime/core/log/Log.h>
#include <runtime/core/math/Math.h>
#include <runtime/core/units/Units.h>
#include <runtime/core/utils/definations.h>
#include <runtime/core/utils/renderdoc/RenderDoc.h>
#include <runtime/core/window/Window.h>

#include <runtime/function/resource/resource_loader/mesh/MeshLoader.h>
#include <runtime/function/resource/resources/mesh/Mesh.h>
#include <runtime/function/rhi/RHI.h>
#include <runtime/function/rhi/RHIUtils.h>
#include <runtime/function/scene/light/Light.h>

#include <runtime/interface/HorizonRuntime.h>

#include <runtime/system/render/RenderSystem.h>

// TODO(hyl5) : move to source file
using namespace Horizon;
using namespace Horizon::Backend;

static constexpr u32 width = 1600, height = 900;

extern std::filesystem::path shader_dir;
extern std::filesystem::path asset_path;
